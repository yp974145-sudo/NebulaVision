import json,urllib.request,urllib.error,subprocess,time,sys,os
from pathlib import Path
x=Path(sys.argv[1]);out=Path(sys.argv[2]);out.mkdir(parents=True,exist_ok=True)
accounts=json.loads((x/'本机登录信息.local.json').read_text(encoding='utf-8-sig'))
def call(path,body=None,token=''):
 headers={'Content-Type':'application/json'}
 if token:headers['Authorization']='Bearer '+token
 req=urllib.request.Request('http://127.0.0.1:18880'+path,data=None if body is None else json.dumps(body).encode(),headers=headers)
 try:
  with urllib.request.urlopen(req,timeout=10) as response:
   data=response.read();return response.status,json.loads(data) if 'json' in response.headers.get('Content-Type','') else data
 except urllib.error.HTTPError as e:return e.code,json.load(e)
admin=call('/api/login',accounts['admin'])[1]['token'];viewer=call('/api/login',accounts['viewer'])[1]['token']
camera=call('/api/cameras',token=admin)[1]['cameras'][0]['id']
assert call('/api/recordings/capture',{'cameraId':camera,'seconds':3},viewer)[0]==403
publish=call('/api/media/publish',{'cameraId':camera},admin)[1]['url']
ffmpeg=next((x/'media-tools/imageio-ffmpeg/imageio_ffmpeg/binaries').glob('ffmpeg*.exe'))
publisher=subprocess.Popen([str(ffmpeg),'-hide_banner','-loglevel','error','-nostdin','-re','-stream_loop','-1','-i',str(x/'samples/test.mp4'),'-c','copy','-rtsp_transport','tcp','-f','rtsp',publish],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,creationflags=subprocess.CREATE_NO_WINDOW)
try:
 time.sleep(1);code,data=call('/api/recordings/capture',{'cameraId':camera,'seconds':3},admin);assert code==202;id=data['id']
 row={}
 for _ in range(40):
  time.sleep(.4);rows=call('/api/recordings?size=100',token=admin)[1]['recordings'];row=next(z for z in rows if z['id']==id)
  if row['status']!='writing':break
 assert row['status']=='ready',row
 code,video=call('/api/recordings/file?id='+id,token=viewer);assert code==200 and len(video)>1000
 file=out/'recording-download.mp4';file.write_bytes(video)
 check=subprocess.run([str(ffmpeg),'-hide_banner','-loglevel','error','-i',str(file),'-frames:v','1','-f','null','-'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=8)
 assert check.returncode==0
 assert call('/api/recordings/file?id='+id)[0]==401
 assert call('/api/recordings/file?id=../bad',token=admin)[0]==400
 # 临时移开本次创建的文件，验证索引存在但文件缺失时返回410，再立即恢复。
 config=json.loads((x.parent/'class15_backup_20261102/NebulaVision/config/server.local.json').read_text(encoding='utf-8-sig'))
 root=Path(config['recording_root']);files=sorted(root.glob('*.mp4'),key=lambda p:p.stat().st_mtime,reverse=True);real=files[0];missing=real.with_suffix('.test-missing')
 real.rename(missing)
 try:assert call('/api/recordings/file?id='+id,token=admin)[0]==410
 finally:missing.rename(real)
 (out/'stage13.json').write_text(json.dumps({'recordingId':id,'status':row['status'],'bytes':len(video),'decode':True,'missingFileHTTP':410}))
 print('PASS: capture, finalize, index, authorized download, decode, permissions, missing-file handling')
finally:publisher.terminate();publisher.wait(timeout=5)
