"""仅在没有其他服务实例监听时运行。验证本项目管理器启动、停止和重启。"""
import sys
sys.dont_write_bytecode = True
import subprocess
import time
import json
import urllib.request
from pathlib import Path
resource = Path(sys.argv[1])
script = Path(__file__).resolve().parents[1] / "scripts/local_demo.py"
state = resource / "runtime-state/local-stack.json"

def start():
    process = subprocess.Popen([sys.executable, str(script), str(resource), "--supervise", "--no-client"],
        creationflags=subprocess.CREATE_NO_WINDOW)
    deadline = time.monotonic() + 30
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise RuntimeError("supervisor stopped before ready")
        if state.exists():
            try:
                if json.loads(state.read_text())["ready"]:
                    with urllib.request.urlopen("http://127.0.0.1:18880/health", timeout=3) as response:
                        assert response.status == 200
                    return process
            except (ValueError, KeyError):
                pass
        time.sleep(.2)
    raise TimeoutError("startup timeout")

for cycle in range(2):
    process = None
    try:
        process = start()
        time.sleep(3)
        assert process.poll() is None
        print("managed stack cycle", cycle + 1, "ready")
    finally:
        subprocess.run([sys.executable, str(script), str(resource), "--stop"], check=True, timeout=30)
        if process:
            process.wait(timeout=10)
    assert not state.exists()
print("PASS: two starts, service health, managed stop, state cleanup")
