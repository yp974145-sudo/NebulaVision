"""运行：配置PYTHONPATH到agent及agent-python；传入本次任务临时目录。"""
import sys
sys.dont_write_bytecode = True
from pathlib import Path
import sqlite3
from workflow import run_query, BusinessTools

class FakeTools:
    def __init__(self, owner="1"):
        self.owner = owner
        self.calls = 0
        self.fail = False
        self.denied = False
    def get(self, path):
        if self.denied:
            raise PermissionError("expired")
        if path == "/api/me":
            return {"id": self.owner}
        self.calls += 1
        if self.fail:
            raise TimeoutError("controlled")
        if path == "/api/cameras":
            return {"cameras": [{"id": "1", "name": "demo"}]}
        return {"events": [{"id": "28", "recordingId": "3"}], "total": 17}

database = Path(sys.argv[1]) / "agent-test.sqlite3"
tools = FakeTools()
tools.fail = True
try:
    run_query(tools, "设备列表", "resume-test", database)
    raise AssertionError("timeout must fail")
except TimeoutError:
    pass
tools.fail = False
result = run_query(tools, "设备列表", "resume-test", database, True)
assert "可见设备数：1" in result["report"]
assert tools.calls == 2
assert "事件总数：17" in run_query(tools, "2026-11-02 事件", "events", database)["report"]
for question in ("删除所有事件", "2026-02-30 事件", "http://evil.test"):
    try:
        run_query(tools, question, "invalid", database)
        raise AssertionError("invalid question accepted")
    except ValueError:
        pass
try:
    run_query(tools, "设备列表", "events", database)
    raise AssertionError("changed question accepted")
except ValueError:
    pass
tools.denied = True
try:
    run_query(tools, "设备列表", "resume-test", database, True)
    raise AssertionError("expired session accepted")
except PermissionError:
    pass
second = FakeTools("2")
run_query(second, "设备列表", "resume-test", database, True)
assert second.calls == 1
with sqlite3.connect(database) as connection:
    assert connection.execute("SELECT COUNT(DISTINCT thread_id) FROM checkpoints").fetchone()[0] >= 3
try:
    BusinessTools("not-persisted").get("/api/logout")
    raise AssertionError("write endpoint accepted")
except ValueError:
    pass
assert b"not-persisted" not in database.read_bytes()
print("PASS: checkpoint recovery, permission recheck, owner isolation, whitelist, totals, invalid inputs")
