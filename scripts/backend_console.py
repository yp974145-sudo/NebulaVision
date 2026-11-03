"""独立后端命令窗口：运行状态与脱敏服务日志；Ctrl+C停止整套本机后台。"""
import sys
sys.dont_write_bytecode=True
import os,json,time,re,subprocess
from pathlib import Path
x=Path(sys.argv[1]).resolve();root=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(x/'runtime-python'))
import psutil
sys.stdout.reconfigure(encoding='utf-8',errors='replace')
runtime=x/'runtime-state';runtime.mkdir(exist_ok=True);identity=runtime/'console.json'
def is_running(item):
    try:
        p=psutil.Process(item['pid']);return abs(p.create_time()-item['created'])<.01 and p.exe().lower()==sys.executable.lower()
    except (psutil.Error,KeyError):return False
def main():
    if identity.exists():
        try:
            if is_running(json.loads(identity.read_text())):return
        except (OSError,ValueError):pass
    identity.write_text(json.dumps({'pid':os.getpid(),'created':psutil.Process().create_time()}))
    print('NebulaVision Backend Console\nAPI: http://127.0.0.1:18880\nCtrl+C: stop backend services.\nUse stop script to stop services reliably.\n',flush=True)
    subprocess.run([sys.executable,str(root/'scripts/demo_launcher.py'),str(x),'--services-only'],check=True)
    offset=0;previous=''
    while True:
        snapshot=subprocess.run([sys.executable,str(root/'scripts/stack_status.py'),str(x)],capture_output=True,text=True,timeout=5,creationflags=subprocess.CREATE_NO_WINDOW)
        data=json.loads(snapshot.stdout);state=' | '.join(s['name']+(': running' if s['alive'] else ': stopped') for s in data['services'])
        if state!=previous:print(state or 'Services stopped.',flush=True);previous=state
        log=runtime/'services.log'
        if log.exists():
            size=log.stat().st_size
            if size<offset:offset=0
            with log.open('rb') as f:
                f.seek(max(offset,size-16000));text=f.read().decode('utf-8',errors='replace');offset=f.tell()
            text=re.sub(r'(?i)(?:rtsp|https?)://\S+','[URL hidden]',text)
            text=re.sub(r'(?i)(bearer\s+|token[=:]\s*|password[=:]\s*|secret[=:]\s*)[^\s,]+',r'\1[hidden]',text)
            if text:print(text,end='',flush=True)
        time.sleep(2)
try:main()
except KeyboardInterrupt:
    print('\nStopping backend...',flush=True)
    subprocess.run([sys.executable,str(root/'scripts/demo_launcher.py'),str(x),'--stop'],timeout=30)
except Exception:
    print('Backend console failed. Check runtime-state/launcher.log.',flush=True)
    time.sleep(5)
    sys.exit(1)
finally:
    try:
        if json.loads(identity.read_text()).get('pid')==os.getpid():identity.unlink(missing_ok=True)
    except (OSError,ValueError):pass
