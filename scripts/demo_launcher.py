"""交互入口：检查服务就绪后再打开客户端，错误明确返回给CMD窗口。"""
import sys
sys.dont_write_bytecode = True
import json
import subprocess
import time
import urllib.request
from pathlib import Path

def main():
    resource = Path(sys.argv[1]).resolve()
    root = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(resource / "runtime-python"))
    import psutil
    state = resource / "runtime-state/local-stack.json"
    runtime = state.parent
    runtime.mkdir(exist_ok=True)
    flags = subprocess.CREATE_NO_WINDOW
    script = root / "scripts/local_demo.py"
    if "--stop" in sys.argv:
        subprocess.run([sys.executable, str(script), str(resource), "--stop"], check=True, timeout=30, creationflags=flags)
        print("Services stopped.", flush=True)
        return
    with (runtime / "launcher.log").open("ab", buffering=0) as log:
        process = subprocess.Popen([sys.executable, str(script), str(resource), "--supervise", "--no-client"],
            creationflags=flags, stdout=log, stderr=log)
    deadline = time.monotonic() + 30
    while time.monotonic() < deadline:
        try:
            data = json.loads(state.read_text())
            items = [data["supervisor"]] + data["services"]
            valid = True
            for item in items:
                actual = psutil.Process(item["pid"])
                if abs(actual.create_time() - item["created"]) >= .01 or actual.exe().lower() != item["exe"].lower():
                    valid = False
            if valid and data["ready"]:
                with urllib.request.urlopen("http://127.0.0.1:18880/health", timeout=3) as response:
                    if response.status != 200:
                        raise OSError("API is not ready")
                # 复用已验证的管理器，由它按正式配置打开客户端。
                subprocess.run([sys.executable, str(script), str(resource)], check=True, timeout=10, creationflags=flags)
                (runtime / "last-error.txt").unlink(missing_ok=True)
                print("Services ready. Client opened.", flush=True)
                return
        except (OSError, ValueError, KeyError, psutil.Error):
            pass
        if process.poll() is not None and process.returncode != 0:
            break
        time.sleep(.2)
    raise RuntimeError("Startup failed. Check runtime-state/last-error.txt and launcher.log; another instance may occupy a port.")

if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print("ERROR: " + str(error), file=sys.stderr)
        sys.exit(1)
