import json,sys,time,uuid,urllib.request,urllib.error
from pathlib import Path
x=Path(sys.argv[1]);accounts=json.loads((x/'本机登录信息.local.json').read_text(encoding='utf-8-sig'))
def call(path,body=None,token=''):
 h={'Content-Type':'application/json'}
 if token:h['Authorization']='Bearer '+token
 req=urllib.request.Request('http://127.0.0.1:18880'+path,data=None if body is None else json.dumps(body).encode(),headers=h)
 try:
  with urllib.request.urlopen(req,timeout=8) as r:return r.status,json.load(r)
 except urllib.error.HTTPError as e:return e.code,json.load(e)
admin=call('/api/login',accounts['admin'])[1]['token'];viewer=call('/api/login',accounts['viewer'])[1]['token']
rows=call('/api/recordings',token=admin)[1]['recordings'];record=next(row for row in rows if row['status']=='ready')['id']
before=call('/api/events',token=admin)[1]['total'];body={'jobKey':'contract_'+uuid.uuid4().hex,'recordingId':record,'threshold':.25}
assert call('/api/detections/start',body,viewer)[0]==403
code,data=call('/api/detections/start',body,admin);assert code==202;id=data['id']
for _ in range(40):
 time.sleep(.2);job=call('/api/detections/job?id='+id,token=admin)[1]
 if job['status'] not in ('queued','running'):break
assert job['status']=='succeeded',job
assert job['result']['synthetic'] is True
assert call('/api/detections/start',body,admin)[1]['reused'] is True
body['threshold']=.5;assert call('/api/detections/start',body,admin)[0]==409
assert call('/api/events',token=admin)[1]['total']==before
print('PASS: C++ API to gRPC detector adapter, persisted job, idempotent retry, changed payload rejected, synthetic result does not create real event')
