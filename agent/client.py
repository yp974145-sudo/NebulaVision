"""Qt通过标准输入交付会话，不在命令行、日志或checkpoint保存令牌。"""
import sys
sys.dont_write_bytecode = True
import json
from pathlib import Path

def main():
    resource = Path(sys.argv[1]).resolve()
    sys.path.insert(0, str(resource / "agent-python"))
    from workflow import BusinessTools, run_query
    request = json.loads(sys.stdin.read(16384))
    if request["question"].startswith("文档 "):
        BusinessTools(request["token"]).get("/api/me")
        sys.path.insert(0, str(resource / "rag-python"))
        from retrieval import search
        documents = Path(__file__).resolve().parents[1] / "NebulaVision/docs"
        return {"report": search(resource, documents, request["question"][3:]), "taskId": request["taskId"]}
    state = resource / "agent-state"
    state.mkdir(exist_ok=True)
    return run_query(BusinessTools(request["token"]), request["question"],
                     request["taskId"], state / "checkpoints.sqlite3", request.get("resume", False))

if __name__ == "__main__":
    try:
        print(json.dumps(main(), ensure_ascii=True))
    except Exception as error:
        # 不把可能含令牌、URL或服务器响应的异常原文输出。
        print(json.dumps({"error": "查询未完成。请检查登录、问题格式与本机服务；可使用原任务编号重试。",
                          "type": type(error).__name__}, ensure_ascii=True))
        sys.exit(1)
