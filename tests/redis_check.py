import json,urllib.request,urllib.error,time,subprocess,sys,socket
from pathlib import Path
x=Path(sys.argv[1]);out=Path(sys.argv[2]);accounts=json.loads((x/'本机登录信息.local.json').read_text(encoding='utf-8-sig'))
def call(path,body=None,token=''):
 headers={'Content-Type':'application/json'}
 if token:headers['Authorization']='Bearer '+token
 req=urllib.request.Request('http://127.0.0.1:18880'+path,data=None if body is None else json.dumps(body).encode(),headers=headers)
 try:
  with urllib.request.urlopen(req,timeout=5) as r:return r.status,json.load(r)
 except urllib.error.HTTPError as e:return e.code,json.load(e)
def redis(*args):
 with socket.create_connection(('127.0.0.1',16379),timeout=1) as s:
  message=b'*'+str(len(args)).encode()+b'\r\n'
  for arg in args:
   arg=str(arg).encode();message+=b'$'+str(len(arg)).encode()+b'\r\n'+arg+b'\r\n'
  s.sendall(message);return s.recv(1024)
exe=next((x/'external/redis').rglob('redis-server.exe'))
def start():return subprocess.Popen([str(exe),'--bind','127.0.0.1','--port','16379','--save','','--appendonly','no'],cwd=out,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,creationflags=subprocess.CREATE_NO_WINDOW)
process=start()
try:
 for _ in range(30):
  try:
   if redis('PING').startswith(b'+PONG'):break
  except OSError:time.sleep(.1)
 token=call('/api/login',accounts['admin'])[1]['token'];uid=call('/api/me',token=token)[1]['id'];camera=call('/api/cameras',token=token)[1]['cameras'][0]['id']
 assert call('/api/cameras/heartbeat',{'cameraId':camera,'ttl':2},token)[0]==200
 assert call('/api/cameras',token=token)[1]['cameras'][0]['status']=='online'
 time.sleep(2.3)
 assert call('/api/cameras',token=token)[1]['cameras'][0]['status']=='offline'
 limited=False
 for _ in range(125):
  if call('/api/me',token=token)[0]==429:limited=True;break
 assert limited
 redis('DEL',f'nv:rate:{uid}:{int(time.time())//60}')
 process.terminate();process.wait(timeout=5)
 cameraData=call('/api/cameras',token=token)[1]['cameras'][0]
 assert cameraData['status']=='unknown' and cameraData['statusSource']=='unavailable'
 assert call('/api/status',token=token)[1]['rateMode']=='local-fallback'
 assert call('/api/events',token=token)[0]==200
 assert call('/api/cameras/heartbeat',{'cameraId':camera,'ttl':2},token)[0]==503
 process=start();time.sleep(.4)
 assert call('/api/status',token=token)[1]['redis']=='available'
 print('PASS: Redis online heartbeat, TTL offline, atomic rate limit 429, outage unknown state, local fallback, recovery')
finally:
 if process.poll() is None:process.terminate();process.wait(timeout=5)
