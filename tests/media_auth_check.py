import json,urllib.request,urllib.error,subprocess,time,sys
from pathlib import Path
x=Path(sys.argv[1]);accounts=json.loads((x/'本机登录信息.local.json').read_text(encoding='utf-8-sig'))
def call(path,body=None,token=''):
 headers={'Content-Type':'application/json'}
 if token:headers['Authorization']='Bearer '+token
 req=urllib.request.Request('http://127.0.0.1:18880'+path,data=None if body is None else json.dumps(body).encode(),headers=headers)
 try:
  with urllib.request.urlopen(req,timeout=10) as response:return response.status,json.load(response)
 except urllib.error.HTTPError as e:return e.code,json.load(e)
admin=call('/api/login',accounts['admin'])[1]['token'];viewer=call('/api/login',accounts['viewer'])[1]['token']
camera=call('/api/cameras',token=admin)[1]['cameras'][0]['id']
assert call('/api/media/publish',{'cameraId':camera},viewer)[0]==403
publish=call('/api/media/publish',{'cameraId':camera},admin)[1]['url']
play=call('/api/media/play',{'cameraId':camera},viewer)[1]['url']
ffmpeg=next((x/'media-tools/imageio-ffmpeg/imageio_ffmpeg/binaries').glob('ffmpeg*.exe'))
publisher=subprocess.Popen([str(ffmpeg),'-hide_banner','-loglevel','error','-nostdin','-re','-stream_loop','-1','-i',str(x/'samples/test.mp4'),'-c','copy','-rtsp_transport','tcp','-f','rtsp',publish],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,creationflags=subprocess.CREATE_NO_WINDOW)
try:
 time.sleep(1)
 def read(url,output):
  return subprocess.run([str(ffmpeg),'-hide_banner','-loglevel','error','-rtsp_transport','tcp','-timeout','3000000','-i',url,'-frames:v','1','-y',str(output)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=10).returncode
 assert read('rtsp://127.0.0.1:18554/demo-camera-01',x/'验证记录/阶段9到16/denied.jpg')!=0
 assert read(play,x/'验证记录/阶段9到16/authorized.jpg')==0
 tampered=play[:-1]+('A' if play[-1]!='A' else 'B');assert read(tampered,x/'验证记录/阶段9到16/tampered.jpg')!=0
 print('PASS: real authorized RTSP frame, anonymous denied, tampered token denied, viewer publish denied')
finally:publisher.terminate();publisher.wait(timeout=5)
