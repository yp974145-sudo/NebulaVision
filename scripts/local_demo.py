"""本机开发栈启动器。只管理自己记录的进程，核对PID创建时间，避免结束其他任务。"""
import sys,os,json,time,subprocess,urllib.request,urllib.error,socket,signal
from pathlib import Path
sys.dont_write_bytecode=True
root=Path(__file__).resolve().parents[1]
x=Path(sys.argv[1]).resolve()
sys.path.insert(0,str(x/'runtime-python'))
import psutil
runtime=x/'runtime-state';runtime.mkdir(exist_ok=True)
state_path=runtime/'local-stack.json'
flags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0

def identity(process,name):
 p=psutil.Process(process.pid)
 return {'pid':p.pid,'created':p.create_time(),'exe':p.exe(),'name':name}
def verified(item):
 try:
  p=psutil.Process(item['pid'])
  return p if abs(p.create_time()-item['created'])<.01 and os.path.normcase(p.exe())==os.path.normcase(item['exe']) else None
 except (psutil.Error,KeyError):return None
def terminate(item,recursive=True):
 p=verified(item)
 if not p:return
 targets=p.children(recursive=True) if recursive else []
 for child in reversed(targets):
  try:child.terminate()
  except psutil.Error:pass
 try:p.terminate()
 except psutil.Error:pass
 psutil.wait_procs(targets+[p],timeout=3)
def load_state():
 try:return json.loads(state_path.read_text())
 except (OSError,ValueError):return {}
def stop_existing():
 old=load_state()
 # 先停守护进程，再根据独立记录结束服务，GUI没有记录在服务集合中。
 if old.get('supervisor'):terminate(old['supervisor'],False)
 for item in reversed(old.get('services',[])):terminate(item)
 state_path.unlink(missing_ok=True)

def api(path,body=None,token=''):
 headers={'Content-Type':'application/json'}
 if token:headers['Authorization']='Bearer '+token
 request=urllib.request.Request('http://127.0.0.1:18880'+path,data=None if body is None else json.dumps(body).encode(),headers=headers)
 with urllib.request.urlopen(request,timeout=4) as response:return json.load(response)
def port_ready(port):
 try:
  with socket.create_connection(('127.0.0.1',port),timeout=.3):return True
 except OSError:return False
def open_client():
 env=os.environ.copy();env['NEBULAVISION_API_URL']='http://127.0.0.1:18880';env['NEBULAVISION_FFMPEG']=str(ffmpeg);env['NEBULAVISION_CACHE_ROOT']=str(x/'playback-cache')
 subprocess.Popen([str(root/'bin-qt6/class15.exe')],env=env)

if '--stop' in sys.argv:stop_existing();sys.exit(0)
ffmpeg=next((x/'media-tools/imageio-ffmpeg/imageio_ffmpeg/binaries').glob('ffmpeg*.exe'))
old=load_state()
if old.get('supervisor') and verified(old['supervisor']):
 if '--no-client' not in sys.argv:open_client()
 sys.exit(0)
# 不接管其他任务或用户已经监听的端口。
if any(port_ready(port) for port in [18880,18554,18551,16379]):
 (runtime/'last-error.txt').write_text('Local service port is already in use. Stop that instance before starting this stack.')
 sys.exit(2)
services=[];processes=[];log=open(runtime/'services.log','ab',buffering=0)
env=os.environ.copy();env['PYTHONPATH']=str(x/'runtime-python');env['PYTHONDONTWRITEBYTECODE']='1'
def spawn(name,arguments,quiet=False):
 process=subprocess.Popen([str(a) for a in arguments],env=env,cwd=runtime,creationflags=flags,stdout=subprocess.DEVNULL if quiet else log,stderr=subprocess.DEVNULL if quiet else log)
 item=identity(process,name);services.append(item);processes.append(process);save_state(False);return process,item
def save_state(ready):
 state={'supervisor':{'pid':os.getpid(),'created':psutil.Process().create_time(),'exe':sys.executable,'name':'supervisor'},'services':services,'ready':ready}
 temporary=state_path.with_suffix('.tmp');temporary.write_text(json.dumps(state,indent=2));temporary.replace(state_path)
try:
 redis=next((x/'external/redis').rglob('redis-server.exe'))
 spawn('redis',[redis,'--bind','127.0.0.1','--port','16379','--save','','--appendonly','no'])
 spawn('api',[root/'bin-qt6/nebulavision_server.exe'])
 spawn('media',[x/'media-tools/mediamtx/mediamtx.exe',x/'media-tools/mediamtx/business.local.yml'])
 spawn('detector',[sys.executable,root/'detector/worker.py',root/'NebulaVision/config/detector.local.json'])
 deadline=time.monotonic()+20
 while not all(port_ready(port) for port in [18880,18554,18551,16379]):
  if time.monotonic()>deadline or any(p.poll() is not None for p in processes):raise RuntimeError('service startup failed')
  time.sleep(.2)
 accounts=json.loads((x/'本机登录信息.local.json').read_text(encoding='utf-8-sig'))
 token=api('/api/login',accounts['admin'])['token'];camera=api('/api/cameras',token=token)['cameras'][0]['id']
 def publish():
  url=api('/api/media/publish',{'cameraId':camera},token)['url']
  return spawn('demo-publisher',[ffmpeg,'-hide_banner','-loglevel','error','-nostdin','-re','-stream_loop','-1','-i',x/'models/person-demo.mp4','-c','copy','-rtsp_transport','tcp','-f','rtsp',url],True)
 publisher,publisher_identity=publish();issued=time.monotonic();save_state(True)
 if '--no-client' not in sys.argv:open_client()
 while True:
  if any(p.poll() is not None for p in processes):raise RuntimeError('a managed service exited')
  api('/api/cameras/heartbeat',{'cameraId':camera,'ttl':6},token)
  if time.monotonic()-issued>2700:
   terminate(publisher_identity);processes.remove(publisher);services.remove(publisher_identity)
   token=api('/api/login',accounts['admin'])['token'];publisher,publisher_identity=publish();issued=time.monotonic();save_state(True)
  time.sleep(2)
except BaseException as error:
 (runtime/'last-error.txt').write_text(type(error).__name__+': local stack stopped; inspect service log.')
finally:
 for item in reversed(services):terminate(item)
 state_path.unlink(missing_ok=True);log.close()
