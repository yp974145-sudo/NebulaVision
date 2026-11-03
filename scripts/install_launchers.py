"""安装可双击的Windows开发启动器；统一ASCII CMD及CRLF，避免混合换行解析失败。"""
import sys
from pathlib import Path

def install(resource):
    root = Path(__file__).resolve().parents[1]
    script = str(root / "scripts/demo_launcher.py").replace("'", "''")
    ps = """param([switch]$Stop)
$ErrorActionPreference = 'Stop'
$taskPython = Join-Path $env:USERPROFILE '.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe'
$taskScript = 'SCRIPT_PATH'
if (!(Test-Path -LiteralPath $taskPython)) { Write-Host 'ERROR: Python runtime is missing.'; exit 1 }
if (!(Test-Path -LiteralPath $taskScript)) { Write-Host 'ERROR: Project launcher is missing.'; exit 1 }
if ($Stop) { & $taskPython $taskScript $PSScriptRoot --stop } else { & $taskPython $taskScript $PSScriptRoot }
exit $LASTEXITCODE
""".replace("SCRIPT_PATH", script)
    resource.mkdir(exist_ok=True)
    for name in ("start-demo.ps1", "启动完整演示.ps1"):
        (resource / name).write_bytes(ps.replace("\n", "\r\n").encode("utf-8-sig"))
    stop = "& (Join-Path $PSScriptRoot 'start-demo.ps1') -Stop\r\nexit $LASTEXITCODE\r\n"
    (resource / "停止本机演示服务.ps1").write_bytes(stop.encode("ascii"))
    for name, argument in [("启动完整演示.cmd", ""), ("启动阶段9到16完整演示.cmd", ""),
                           ("停止完整演示.cmd", " -Stop"), ("停止阶段9到16演示服务.cmd", " -Stop")]:
        lines = ["@echo off", 'powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0start-demo.ps1"' + argument,
                 'set "launch_rc=%errorlevel%"', 'if not "%launch_rc%"=="0" pause', 'exit /b %launch_rc%']
        (resource / name).write_bytes(("\r\n".join(lines) + "\r\n").encode("ascii"))
    print("Windows launchers installed.")

if __name__ == "__main__":
    install(Path(sys.argv[1]).resolve())
