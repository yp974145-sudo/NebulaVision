"""Windows本机隔离回归，只删除本次创建的设备、事件、录像和文件。"""
import sys
sys.dont_write_bytecode=True
import os,json,subprocess,urllib.request,uuid,hashlib
from pathlib import Path
x,out,executable=map(Path,sys.argv[1:4]);root=Path(__file__).resolve().parents[1];config=json.loads((root/'NebulaVision/config/server.local.json').read_text(encoding='utf-8-sig'));accounts=json.loads((x/'本机登录信息.local.json').read_text(encoding='utf-8-sig'))
def call(path,body,token=''):
    req=urllib.request.Request('http://127.0.0.1:18880'+path,data=json.dumps(body).encode(),headers={'Content-Type':'application/json','Authorization':'Bearer '+token})
    with urllib.request.urlopen(req,timeout=15) as response:return json.load(response)
env=os.environ.copy();env['MYSQL_PWD']=config['password']
def sql(text):
    return subprocess.run([r'E:\Program Files\MySQL\MySQL Server 8.0\bin\mysql.exe','--host=127.0.0.1','--user='+config['username'],'--database=nebulavision_dev','--batch','--skip-column-names'],input=text,text=True,env=env,capture_output=True,check=True).stdout.splitlines()
admin=call('/api/login',accounts['admin'])['token'];cameras=[]
try:
    for _ in range(2):
        name='check_retry_'+uuid.uuid4().hex[:12];camera=call('/api/cameras/register',{'name':name,'sourceType':'local','sourceKey':hashlib.sha256(name.encode()).hexdigest()},admin)['id'];assert camera.isdigit();cameras.append((camera,name))
    ffmpeg=next((x/'media-tools/imageio-ffmpeg/imageio_ffmpeg/binaries').glob('ffmpeg*.exe'));env['NEBULAVISION_FFMPEG']=str(ffmpeg);env['NEBULAVISION_TEST_OUTPUT']=str(out);env['NEBULAVISION_CACHE_ROOT']=str(out/'playback-cache')
    subprocess.run([str(executable),str(x),*[camera for camera,name in cameras]],env=env,check=True,timeout=150)
finally:
    for camera,name in cameras:
        files=sql('SELECT object_key FROM recordings WHERE camera_id='+camera)
        sql("START TRANSACTION; DELETE a FROM audit_logs a JOIN events e ON a.resource_type='event' AND a.resource_id=CAST(e.id AS CHAR) WHERE e.camera_id=%s; DELETE a FROM audit_logs a JOIN recordings r ON a.resource_type='recording' AND a.resource_id=CAST(r.id AS CHAR) WHERE r.camera_id=%s; DELETE FROM live_event_receipts WHERE camera_id=%s; DELETE FROM events WHERE camera_id=%s; DELETE FROM recordings WHERE camera_id=%s; DELETE FROM cameras WHERE id=%s AND name='%s'; COMMIT;"%(camera,camera,camera,camera,camera,camera,name))
        for filename in files:
            assert Path(filename).name==filename
            (Path(config['recording_root'])/filename).unlink(missing_ok=True)
    call('/api/logout',{},admin)
