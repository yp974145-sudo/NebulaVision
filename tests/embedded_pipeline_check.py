"""创建独立RTSP手势样本源，运行嵌入通道+真实入库检查，最后清理本次网络设备与事件。"""
import sys
sys.dont_write_bytecode=True
import json,urllib.request,urllib.error,subprocess,time,uuid,os
from pathlib import Path
x,out,executable=map(Path,sys.argv[1:4]);root=Path(__file__).resolve().parents[1];out.mkdir(exist_ok=True)
accounts=json.loads((x/'本机登录信息.local.json').read_text(encoding='utf-8-sig'))
def call(path,body=None,token=''):
    headers={'Content-Type':'application/json'}
    if token:headers['Authorization']='Bearer '+token
    req=urllib.request.Request('http://127.0.0.1:18880'+path,data=None if body is None else json.dumps(body).encode(),headers=headers)
    with urllib.request.urlopen(req,timeout=12) as response:return json.load(response)
config=json.loads((root/'NebulaVision/config/server.local.json').read_text(encoding='utf-8-sig'));db_env=os.environ.copy();db_env['MYSQL_PWD']=config['password']
def sql(text):subprocess.run([r'E:\Program Files\MySQL\MySQL Server 8.0\bin\mysql.exe','--host=127.0.0.1','--user='+config['username'],'--database=nebulavision_dev'],input=text,text=True,env=db_env,check=True,stdout=subprocess.DEVNULL)
image=out/'hands.jpg'
if not image.exists():urllib.request.urlretrieve('https://storage.googleapis.com/mediapipe-assets/right_hands.jpg',image)
ffmpeg=next((x/'media-tools/imageio-ffmpeg/imageio_ffmpeg/binaries').glob('ffmpeg*.exe'));clip=out/'hands.mp4'
subprocess.run([str(ffmpeg),'-hide_banner','-loglevel','error','-loop','1','-i',str(image),'-t','14','-c:v','libx264','-tune','zerolatency','-g','25','-crf','10','-pix_fmt','yuv420p','-y',str(clip)],check=True,timeout=20)
admin=call('/api/login',accounts['admin'])['token'];name='check_embedded_'+uuid.uuid4().hex[:10];camera=None;publisher=None
try:
    camera=call('/api/cameras/register',{'name':name,'sourceType':'rtsp','url':'rtsp://127.0.0.1:18554/'+name},admin)['id'];assert camera.isdigit()
    publish=call('/api/media/publish',{'cameraId':camera},admin)['url'];read=call('/api/media/play',{'cameraId':camera},admin)['url']
    sql("UPDATE cameras SET stream_url='%s' WHERE id=%s"%(read.replace("'","''"),camera))
    publisher=subprocess.Popen([str(ffmpeg),'-hide_banner','-loglevel','error','-nostdin','-re','-stream_loop','-1','-i',str(clip),'-c','copy','-rtsp_transport','tcp','-f','rtsp',publish],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,creationflags=subprocess.CREATE_NO_WINDOW)
    time.sleep(1);env=os.environ.copy();env.update({'NEBULAVISION_PROJECT_ROOT':str(root),'NEBULAVISION_RESOURCES_ROOT':str(x),'NEBULAVISION_AGENT_PYTHON':sys.executable,'NEBULAVISION_FFMPEG':str(ffmpeg),'NEBULAVISION_TEST_OUTPUT':str(out)})
    result=subprocess.run([str(executable),str(x),camera],env=env,timeout=90);assert result.returncode==0,result.returncode
finally:
    if publisher:publisher.terminate();publisher.wait(timeout=5)
    try:call('/api/logout',{},admin)
    except Exception:pass
    if camera:
        files=subprocess.run([r'E:\Program Files\MySQL\MySQL Server 8.0\bin\mysql.exe','--host=127.0.0.1','--user='+config['username'],'--database=nebulavision_dev','--batch','--skip-column-names','--execute=SELECT object_key FROM recordings WHERE camera_id='+camera],text=True,env=db_env,check=True,capture_output=True).stdout.splitlines()
        sql("START TRANSACTION; DELETE a FROM audit_logs a JOIN events e ON a.resource_type='event' AND a.resource_id=CAST(e.id AS CHAR) WHERE e.camera_id=%s; DELETE a FROM audit_logs a JOIN recordings r ON a.resource_type='recording' AND a.resource_id=CAST(r.id AS CHAR) WHERE r.camera_id=%s; DELETE FROM live_event_receipts WHERE camera_id=%s; DELETE FROM events WHERE camera_id=%s; DELETE FROM recordings WHERE camera_id=%s; DELETE FROM cameras WHERE id=%s AND name='%s'; COMMIT;"%(camera,camera,camera,camera,camera,camera,name))
        for filename in files:
            assert Path(filename).name==filename
            (Path(config['recording_root'])/filename).unlink(missing_ok=True)
