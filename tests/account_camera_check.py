"""管理员账号创建、设备权限与当前演示流检测；只输出非秘密测试结果。"""
import sys
sys.dont_write_bytecode=True
import json,urllib.request,urllib.error,time,uuid,subprocess,os
from pathlib import Path
x=Path(sys.argv[1]);out=Path(sys.argv[2]);root=Path(__file__).resolve().parents[1]
accounts=json.loads((x/'本机登录信息.local.json').read_text(encoding='utf-8-sig'))
def call(path,body=None,token=''):
    headers={'Content-Type':'application/json'}
    if token:headers['Authorization']='Bearer '+token
    request=urllib.request.Request('http://127.0.0.1:18880'+path,data=None if body is None else json.dumps(body).encode(),headers=headers)
    try:
        with urllib.request.urlopen(request,timeout=12) as response:return response.status,json.load(response)
    except urllib.error.HTTPError as error:return error.code,json.load(error)
admin=call('/api/login',accounts['admin'])[1]['token'];viewer=call('/api/login',accounts['viewer'])[1]['token']
camera=call('/api/cameras',token=admin)[1]['cameras'][0]['id'];name='check_'+uuid.uuid4().hex[:16]
password=uuid.uuid4().hex;user_id=None;new_token=''
try:
    body={'username':name,'password':password,'cameraIds':[camera]}
    assert call('/api/users',body)[0]==401
    assert call('/api/users',body,viewer)[0]==403
    assert call('/api/users',{**body,'cameraIds':['-1']},admin)[0]==400
    code,created=call('/api/users',body,admin);assert code==201,(code,created);user_id=created['id']
    assert call('/api/users',body,admin)[0]==409
    new_token=call('/api/login',{'username':name,'password':password})[1]['token']
    assert call('/api/me',token=new_token)[1]['admin'] is False
    assert any(c['id']==camera for c in call('/api/cameras',token=new_token)[1]['cameras'])
    assert call('/api/recordings/capture',{'cameraId':camera,'seconds':3},new_token)[0]==403
    print('PASS: create viewer, duplicate handling, anonymous/viewer denied, assigned camera visible')
    url=call('/api/media/play',{'cameraId':camera},new_token)[1]['url']
    ffmpeg=next((x/'media-tools/imageio-ffmpeg/imageio_ffmpeg/binaries').glob('ffmpeg*.exe'))
    result=subprocess.run([str(ffmpeg),'-hide_banner','-loglevel','error','-rtsp_transport','tcp','-timeout','5000000','-i',url,'-frames:v','1','-y',str(out/'demo-frame.png')],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=12)
    assert result.returncode==0
    print('PASS: authorized demo RTSP decoded a frame')
    code,record=call('/api/recordings/capture',{'cameraId':camera,'seconds':3},admin);assert code==202
    for _ in range(50):
        time.sleep(.3);rows=call('/api/recordings?size=100',token=admin)[1]['recordings'];row=next(r for r in rows if r['id']==record['id'])
        if row['status']!='writing':break
    assert row['status']=='ready',row
    code,job=call('/api/detections/start',{'recordingId':record['id'],'jobKey':uuid.uuid4().hex,'threshold':.25},admin);assert code==202
    for _ in range(50):
        time.sleep(.3);status=call('/api/detections/job?id='+job['id'],token=admin)[1]
        if status['status'] not in ('queued','running'):break
    assert status['status']=='succeeded',status
    result=status['result'];assert result['synthetic'] is False and len(result['detections'])>=1
    (out/'result.json').write_text(json.dumps({'recordingId':record['id'],'eventId':result.get('eventId'),'people':len(result['detections'])}))
    print('PASS: recorded demo video -> real YOLO -> people='+str(len(result['detections']))+' recording='+record['id']+' event='+str(result.get('eventId')))
finally:
    for token in (new_token,admin,viewer):
        if token:call('/api/logout',{},token)
    if user_id:
        assert str(user_id).isdigit()
        config=json.loads((root/'NebulaVision/config/server.local.json').read_text(encoding='utf-8-sig'))
        env=os.environ.copy();env['MYSQL_PWD']=config['password']
        sql="START TRANSACTION; DELETE FROM user_camera_permissions WHERE user_id=%s; DELETE FROM user_roles WHERE user_id=%s; DELETE FROM users WHERE id=%s AND username='%s'; COMMIT;"%(user_id,user_id,user_id,name)
        subprocess.run([r'E:\Program Files\MySQL\MySQL Server 8.0\bin\mysql.exe','-h127.0.0.1','-u'+config['username'],'nebulavision_dev'],input=sql,text=True,env=env,check=True,stdout=subprocess.DEVNULL)
