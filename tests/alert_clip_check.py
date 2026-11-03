"""异常录像上传的正反用例；只清理本次隔离设备的数据及文件。"""
import sys
sys.dont_write_bytecode=True
import json,urllib.request,urllib.error,uuid,hashlib,subprocess,os,base64
from pathlib import Path
x,clip=map(Path,sys.argv[1:3]);root=Path(__file__).resolve().parents[1]
accounts=json.loads((x/'本机登录信息.local.json').read_text(encoding='utf-8-sig'))
config=json.loads((root/'NebulaVision/config/server.local.json').read_text(encoding='utf-8-sig'));env=os.environ.copy();env['MYSQL_PWD']=config['password']
def call(path,body=None,token=''):
    req=urllib.request.Request('http://127.0.0.1:18880'+path,data=None if body is None else json.dumps(body).encode(),headers={'Content-Type':'application/json','Authorization':'Bearer '+token})
    try:
        with urllib.request.urlopen(req,timeout=30) as response:
            raw=response.read();return response.status,json.loads(raw) if 'json' in response.headers.get('Content-Type','') else raw
    except urllib.error.HTTPError as e:return e.code,json.load(e)
def sql(statement,capture=False):
    r=subprocess.run([r'E:\Program Files\MySQL\MySQL Server 8.0\bin\mysql.exe','--host=127.0.0.1','--user='+config['username'],'--database=nebulavision_dev','--batch','--skip-column-names'],input=statement,text=True,env=env,check=True,capture_output=True)
    return r.stdout.splitlines()
admin=call('/api/login',accounts['admin'])[1]['token'];viewer=call('/api/login',accounts['viewer'])[1]['token'];camera=None
try:
    name='check_clip_'+uuid.uuid4().hex[:10]
    camera=call('/api/cameras/register',{'name':name,'sourceType':'local','sourceKey':hashlib.sha256(name.encode()).hexdigest()},admin)[1]['id'];assert camera.isdigit()
    key=str(uuid.uuid4());event=call('/api/events/gesture',{'cameraId':camera,'eventKey':key,'gesture':'Open_Palm','confidence':.9,'consecutive':3},admin)[1]['id']
    body={'eventKey':key,'preFrames':5,'frames':40,'video':base64.b64encode(clip.read_bytes()).decode()}
    assert call('/api/events/clip',body)[0]==401
    assert call('/api/events/clip',body,viewer)[0]==403
    assert call('/api/events/clip',{**body,'preFrames':99},admin)[0]==400
    assert call('/api/events/clip',{**body,'video':base64.b64encode(b'not-a-video'*100).decode()},admin)[0]==422
    code,data=call('/api/events/clip',body,admin);assert code==201,(code,data)
    recording=data['recordingId'];assert call('/api/events/clip',body,admin)[1]['recordingId']==recording
    row=next(e for e in call('/api/events?size=100',token=admin)[1]['events'] if e['id']==event);assert row['recordingId']==recording
    assert call('/api/recordings/file?id='+recording)[0]==401
    assert call('/api/recordings/file?id='+recording,token=viewer)[0]==404
    code,video=call('/api/recordings/file?id='+recording,token=admin);assert code==200 and len(video)>1000
    print('PASS: clip ownership, invalid video denied, transactional event link, retry idempotency, authorized download')
finally:
    if camera:
        files=sql('SELECT object_key FROM recordings WHERE camera_id='+camera)
        sql("START TRANSACTION; DELETE a FROM audit_logs a JOIN events e ON a.resource_type='event' AND a.resource_id=CAST(e.id AS CHAR) WHERE e.camera_id=%s; DELETE a FROM audit_logs a JOIN recordings r ON a.resource_type='recording' AND a.resource_id=CAST(r.id AS CHAR) WHERE r.camera_id=%s; DELETE FROM live_event_receipts WHERE camera_id=%s; DELETE FROM events WHERE camera_id=%s; DELETE FROM recordings WHERE camera_id=%s; DELETE FROM cameras WHERE id=%s AND name='%s'; COMMIT;"%(camera,camera,camera,camera,camera,camera,name))
        for filename in files:
            assert Path(filename).name==filename
            (Path(config['recording_root'])/filename).unlink(missing_ok=True)
    for token in (admin,viewer):
        try:call('/api/logout',{},token)
        except OSError:pass
