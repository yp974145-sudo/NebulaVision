import json,sys,time,uuid,urllib.request,urllib.error,subprocess
from pathlib import Path
x=Path(sys.argv[1]);out=Path(sys.argv[2]);accounts=json.loads((x/'本机登录信息.local.json').read_text(encoding='utf-8-sig'))
def call(path,body=None,token=''):
 h={'Content-Type':'application/json'}
 if token:h['Authorization']='Bearer '+token
 req=urllib.request.Request('http://127.0.0.1:18880'+path,data=None if body is None else json.dumps(body).encode(),headers=h)
 try:
  with urllib.request.urlopen(req,timeout=10) as r:return r.status,json.load(r)
 except urllib.error.HTTPError as e:return e.code,json.load(e)
token=call('/api/login',accounts['admin'])[1]['token'];viewer=call('/api/login',accounts['viewer'])[1]['token']
camera=call('/api/cameras',token=token)[1]['cameras'][0]['id'];old=call('/api/recordings?size=100',token=token)[1]['recordings'];negative=next(z['id'] for z in old if z['status']=='ready')
publish=call('/api/media/publish',{'cameraId':camera},token)[1]['url'];ffmpeg=next((x/'media-tools/imageio-ffmpeg/imageio_ffmpeg/binaries').glob('ffmpeg*.exe'))
publisher=subprocess.Popen([str(ffmpeg),'-hide_banner','-loglevel','error','-nostdin','-re','-stream_loop','-1','-i',str(x/'models/person-demo.mp4'),'-c','copy','-rtsp_transport','tcp','-f','rtsp',publish],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,creationflags=subprocess.CREATE_NO_WINDOW)
try:
 time.sleep(1);code,data=call('/api/recordings/capture',{'cameraId':camera,'seconds':3},token);assert code==202;recording=data['id']
 for _ in range(40):
  time.sleep(.3);rows=call('/api/recordings?size=100',token=token)[1]['recordings'];row=next(z for z in rows if z['id']==recording)
  if row['status']!='writing':break
 assert row['status']=='ready',row
 before=call('/api/events',token=token)[1]['total']
 def detect(record,roi=None,key=None):
  body={'jobKey':key or 'real_'+uuid.uuid4().hex,'recordingId':record,'threshold':.25}
  if roi is not None:body['roi']=roi
  code,job=call('/api/detections/start',body,token);assert code in (200,202),(code,job)
  for _ in range(40):
   time.sleep(.2);result=call('/api/detections/job?id='+job['id'],token=token)[1]
   if result['status'] not in ('queued','running'):break
  assert result['status']=='succeeded',result
  return body,result['result']
 body,result=detect(recording);assert result['synthetic'] is False and len(result['detections'])>=1 and result['eventId']
 assert call('/api/events',token=token)[1]['total']==before+1
 assert call('/api/detections/start',body,token)[1]['reused'] is True
 _,duplicate=detect(recording);assert duplicate['eventId']==result['eventId'] and duplicate['deduplicated'] is True
 _,outside=detect(recording,[0,0,.03,.03]);assert outside['detections']==[]
 _,neg=detect(negative);assert neg['detections']==[]
 events=call('/api/events?size=100',token=viewer)[1]['events'];event=next(e for e in events if e['id']==result['eventId']);assert event['recordingId']==recording
 assert call('/api/detections/start',{'jobKey':'invalid_roi_key','recordingId':recording,'roi':['bad',0,1,1]},token)[0]==400
 assert call('/api/events',token=token)[1]['total']==before+1
 summary={'recordingId':recording,'eventId':result['eventId'],'model':result['modelVersion'],'people':len(result['detections']),'positiveScores':[round(b['confidence'],3) for b in result['detections']],'negativePeople':len(neg['detections']),'roiOutsidePeople':len(outside['detections']),'deduplicated':True}
 (out/'stage16.json').write_text(json.dumps(summary,indent=2));print('PASS: real ONNX video inference, ROI exclusion, negative sample, event/evidence association, job retry and event deduplication')
finally:publisher.terminate();publisher.wait(timeout=5)
