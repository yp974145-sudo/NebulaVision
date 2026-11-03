"""本机集成检查，不打印账号密码或令牌；参数：外部资源目录、测试状态目录。"""
import sys
sys.dont_write_bytecode = True
import json
import urllib.request
from pathlib import Path
from workflow import BusinessTools, run_query

resource, temporary = map(Path, sys.argv[1:3])
accounts = json.loads((resource / "本机登录信息.local.json").read_text(encoding="utf-8-sig"))
for role in ("admin", "viewer"):
    request = urllib.request.Request("http://127.0.0.1:18880/api/login",
        data=json.dumps(accounts[role]).encode(), headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(request, timeout=8) as response:
        token = json.load(response)["token"]
    try:
        tools = BusinessTools(token)
        expected = len(tools.get("/api/cameras")["cameras"])
        result = run_query(tools, "设备列表", "live-" + role, temporary / "live.sqlite3")
        assert "可见设备数：" + str(expected) in result["report"]
        day = "2026-11-02"
        total = tools.get("/api/events?date=" + day + "&page=1&size=10")["total"]
        result = run_query(tools, day + " 事件", "live-events-" + role, temporary / "live.sqlite3")
        assert "事件总数：" + str(total) in result["report"]
        print(role + ": API count reconciliation passed")
    finally:
        request = urllib.request.Request("http://127.0.0.1:18880/api/logout", data=b"{}",
            headers={"Authorization": "Bearer " + token, "Content-Type": "application/json"})
        with urllib.request.urlopen(request, timeout=8):
            pass
