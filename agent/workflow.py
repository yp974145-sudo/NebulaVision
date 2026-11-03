"""受限查询状态图：不生成SQL，不执行写操作，不保存登录令牌。"""
import hashlib
import json
import re
import urllib.request
from datetime import datetime, timezone
from typing import TypedDict
from langgraph.graph import StateGraph, START, END
from langgraph.checkpoint.sqlite import SqliteSaver


class QueryState(TypedDict, total=False):
    question: str
    path: str
    evidence: dict
    report: str


class BusinessTools:
    def __init__(self, token, base="http://127.0.0.1:18880"):
        # 当前只开放本机API，不能由问题内容指定任意URL。
        if base != "http://127.0.0.1:18880":
            raise ValueError("仅允许本机业务服务")
        self.base = base
        self.token = token

    def get(self, path):
        if path != "/api/me" and path != "/api/cameras" and not re.fullmatch(
                r"/api/events\?date=\d{4}-\d{2}-\d{2}&page=1&size=10", path):
            raise ValueError("工具不在只读白名单中")
        request = urllib.request.Request(self.base + path,
                    headers={"Authorization": "Bearer " + self.token})
        with urllib.request.urlopen(request, timeout=5) as response:
            body = response.read(1024 * 1024 + 1)
        if len(body) > 1024 * 1024:
            raise ValueError("响应过大")
        return json.loads(body)


def parse_question(state):
    question = state["question"].strip()
    if question in ("设备", "设备列表", "查询设备"):
        return {"path": "/api/cameras"}
    if question in ("今日事件", "今天事件"):
        day = datetime.now(timezone.utc).date().isoformat()
    else:
        match = re.fullmatch(r"(\d{4}-\d{2}-\d{2})[ ]*事件", question)
        if not match:
            raise ValueError("支持：设备列表、今日事件、YYYY-MM-DD 事件")
        day = match.group(1)
        datetime.strptime(day, "%Y-%m-%d")
    return {"path": "/api/events?date=" + day + "&page=1&size=10"}


def build_graph(tools, saver):
    def query(state):
        # 请求仍由后端核验当前账号权限，不在助手中绕过权限查库。
        return {"evidence": tools.get(state["path"])}

    def verify(state):
        data = state["evidence"]
        field = "cameras" if state["path"] == "/api/cameras" else "events"
        if not isinstance(data.get(field), list):
            raise ValueError("证据字段不完整，不能生成报告")
        if field == "events" and (type(data.get("total")) is not int or data["total"] < len(data[field])):
            raise ValueError("事件总数证据无效")
        return {}

    def report(state):
        data = state["evidence"]
        lines = ["查询：" + state["question"], "来源：GET " + state["path"],
                 "查询时间（UTC）：" + datetime.now(timezone.utc).isoformat()]
        if state["path"] == "/api/cameras":
            lines.append("当前账号可见设备数：" + str(len(data["cameras"])))
            for camera in data["cameras"]:
                lines.append("设备 #" + str(camera.get("id")) + " " + str(camera.get("name", "")))
        else:
            lines.append("当前账号可见事件总数：" + str(data["total"]))
            lines.append("以下最多展示10条，不能把展示数量当成总数。日期按UTC解释。")
            for event in data["events"]:
                # 外部描述只当数据展示，不作为下一步工具调用指令。
                lines.append("事件 #" + str(event.get("id")) + " " + str(event.get("occurredAt", "")) +
                             " 录像 #" + str(event.get("recordingId", "无")))
        lines.append("此为规则解析与证据模板报告，未调用大语言模型。")
        return {"report": "\n".join(lines)}

    graph = StateGraph(QueryState)
    graph.add_node("parse", parse_question)
    graph.add_node("query", query)
    graph.add_node("verify", verify)
    graph.add_node("report", report)
    graph.add_edge(START, "parse")
    graph.add_edge("parse", "query")
    graph.add_edge("query", "verify")
    graph.add_edge("verify", "report")
    graph.add_edge("report", END)
    return graph.compile(checkpointer=saver)


def run_query(tools, question, task_id, database, resume=False):
    if not question or len(question) > 200 or not re.fullmatch(r"[a-zA-Z0-9_-]{1,64}", task_id):
        raise ValueError("问题或任务编号无效")
    # 每次恢复也必须验证会话；令牌只存在tools中，不进入checkpoint。
    identity = tools.get("/api/me")
    owner = str(identity["id"])
    thread = hashlib.sha256((owner + ":" + task_id).encode()).hexdigest()
    config = {"configurable": {"thread_id": thread}}
    with SqliteSaver.from_conn_string(str(database)) as saver:
        graph = build_graph(tools, saver)
        previous = graph.get_state(config)
        if previous.values and previous.values.get("question") != question:
            raise ValueError("同一任务编号不能更换问题，请新建任务")
        if resume and previous.next:
            result = graph.invoke(None, config)
        else:
            # 已完成任务重新查询，避免把旧报告当成实时数据。
            result = graph.invoke({"question": question}, config)
        return {"report": result["report"], "taskId": task_id}
