"""本机后台控制窗口的只读状态快照，不把本地配置和密钥带到界面。"""
import sys
sys.dont_write_bytecode=True
import json,re
from pathlib import Path
x=Path(sys.argv[1]);sys.path.insert(0,str(x/'runtime-python'))
import psutil
try:
    state=json.loads((x/'runtime-state/local-stack.json').read_text())
except (OSError,ValueError):state={}
services=[]
for item in ([state['supervisor']] if state.get('supervisor') else [])+state.get('services',[]):
    try:
        p=psutil.Process(item['pid'])
        alive=abs(p.create_time()-item['created'])<.01 and p.exe().lower()==item['exe'].lower()
    except (psutil.Error,KeyError):alive=False
    services.append({'name':item.get('name','unknown'),'pid':item.get('pid',0),'alive':alive})
log=x/'runtime-state/services.log'
try:
    with log.open('rb') as f:
        f.seek(max(0,log.stat().st_size-16000));text=f.read(16000).decode('utf-8',errors='replace')
except OSError:text='暂无服务日志'
text=re.sub(r'(?i)(?:rtsp|https?)://\S+','[URL已隐藏]',text)
text=re.sub(r'(?i)(bearer\s+|token[=:]\s*|password[=:]\s*|secret[=:]\s*)[^\s,]+',r'\1[已隐藏]',text)
print(json.dumps({'services':services,'ready':bool(state.get('ready')) and bool(services) and all(s['alive'] for s in services),'log':text},ensure_ascii=True))
