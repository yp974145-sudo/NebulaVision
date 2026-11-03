"""新库实时事件鉴权、持久化、冷却合并与跨冷却重试检查；清理本次独立设备。"""
import sys
sys.dont_write_bytecode=True
import json,urllib.request,urllib.error,uuid,hashlib,time,subprocess,os
from pathlib import Path
x=Path(sys.argv[1]);root=Path(__file__).resolve().parents[1]
accounts=json.loads((x/'本机登录信息.local.json').read_text(encoding='utf-8-sig'))
def call(path,body=None,token=''):
    headers={'Content-Type':'application/json'}
    if token:headers['Authorization']='Bearer '+token
    request=urllib.request.Request('http://127.0.0.1:18880'+path,data=None if body is None else json.dumps(body).encode(),headers=headers)
    try:
        with urllib.request.urlopen(request,timeout=10) as response:return response.status,json.load(response)
    except urllib.error.HTTPError as e:return e.code,json.load(e)
admin=call('/api/login',accounts['admin'])[1]['token'];viewer=call('/api/login',accounts['viewer'])[1]['token']
name='check_gesture_'+uuid.uuid4().hex[:10];camera=None
try:
    device={'name':name,'sourceType':'local','sourceKey':hashlib.sha256(name.encode()).hexdigest()}
    assert call('/api/cameras/register',device,viewer)[0]==403
    code,data=call('/api/cameras/register',device,admin);assert code==201,(code,data);camera=data['id']
    assert call('/api/cameras/register',device,admin)[1]['id']==camera
    body={'cameraId':camera,'eventKey':str(uuid.uuid4()),'gesture':'Open_Palm','confidence':.85,'consecutive':3}
    assert call('/api/events/gesture',body)[0]==401
    assert call('/api/events/gesture',body,viewer)[0]==403
    assert call('/api/events/gesture',{**body,'confidence':.4},admin)[0]==400
    code,first=call('/api/events/gesture',body,admin);assert code==201,(code,first)
    assert call('/api/events/gesture',body,admin)[1]['id']==first['id']
    merge={**body,'eventKey':str(uuid.uuid4())};assert call('/api/events/gesture',merge,admin)[1]['id']==first['id']
    time.sleep(11)
    assert call('/api/events/gesture',merge,admin)[1]['id']==first['id']
    rows=call('/api/events?size=100',token=admin)[1]['events'];event=next(e for e in rows if e['id']==first['id']);assert event['type']=='gesture_open_palm' and not event['recordingId']
    assert all(e['id']!=first['id'] for e in call('/api/events?size=100',token=viewer)[1]['events'])
    print('PASS: device upsert, gesture insert, retry idempotency, cooldown merge, post-cooldown retry, read isolation, anonymous/viewer denied')
finally:
    for token in (admin,viewer):call('/api/logout',{},token)
    if camera:
        assert camera.isdigit();config=json.loads((root/'NebulaVision/config/server.local.json').read_text(encoding='utf-8-sig'));env=os.environ.copy();env['MYSQL_PWD']=config['password']
        sql="START TRANSACTION; DELETE a FROM audit_logs a JOIN events e ON a.resource_type='event' AND a.resource_id=CAST(e.id AS CHAR) WHERE e.camera_id=%s; DELETE FROM live_event_receipts WHERE camera_id=%s; DELETE FROM events WHERE camera_id=%s; DELETE FROM cameras WHERE id=%s AND name='%s'; COMMIT;"%(camera,camera,camera,camera,name)
        subprocess.run([r'E:\Program Files\MySQL\MySQL Server 8.0\bin\mysql.exe','--host=127.0.0.1','--user='+config['username'],'--database=nebulavision_dev'],input=sql,text=True,env=env,check=True,stdout=subprocess.DEVNULL)
