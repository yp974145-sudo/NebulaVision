"""仅用于干净、隔离的Linux CI。随机测试账号，不读取开发者本机配置。"""
import sys,os,json,subprocess,time,secrets,hashlib,base64,urllib.request,urllib.error,shutil,uuid,platform,socket
from pathlib import Path
if sys.platform!='linux' or os.environ.get('NEBULAVISION_CI')!='1':sys.exit('Requires isolated Linux CI with NEBULAVISION_CI=1')
root=Path(__file__).resolve().parents[1];build=Path(sys.argv[1]).resolve();out=Path(os.environ['RUNNER_TEMP'])/'nebulavision-acceptance';out.mkdir(exist_ok=True)
resources=out/'resources';resources.mkdir(exist_ok=True);(resources/'models').mkdir(exist_ok=True);(resources/'recordings').mkdir(exist_ok=True)
report={'system':platform.platform(),'checks':{},'not_tested':['physical USB cameras','Seetong cloud access','MediaMTX RTSP relay','full Agent/RAG/DeepSeek flow','long-running stability']};processes=[];logs=[]
def passed(name):report['checks'][name]='PASS';print('PASS:',name,flush=True)
def run(args,**kwargs):return subprocess.run([str(a) for a in args],check=True,**kwargs)
def sql(statement):
    env=os.environ.copy();env['MYSQL_PWD']=os.environ.get('CI_MYSQL_ROOT_PASSWORD','root')
    result=subprocess.run(['mysql','--host=127.0.0.1','--user=root','--batch','--skip-column-names'],input=statement,text=True,capture_output=True,env=env)
    if result.returncode:raise RuntimeError('CI database setup/query failed; SQL and credentials omitted')
    return result.stdout.strip()
def call(path,body=None,token=''):
    req=urllib.request.Request('http://127.0.0.1:18880'+path,data=None if body is None else json.dumps(body).encode(),headers={'Content-Type':'application/json','Authorization':'Bearer '+token})
    try:
        with urllib.request.urlopen(req,timeout=30) as response:
            raw=response.read();return response.status,json.loads(raw) if 'json' in response.headers.get('Content-Type','') else raw
    except urllib.error.HTTPError as e:return e.code,json.load(e)
def spawn(name,args,env=None):
    log=(out/(name+'.log')).open('ab');logs.append(log);p=subprocess.Popen([str(a) for a in args],stdout=log,stderr=log,env=env);processes.append(p);return p
def wait(condition,timeout=30):
    deadline=time.monotonic()+timeout
    while time.monotonic()<deadline:
        try:
            if condition():return
        except (OSError,KeyError,ValueError):pass
        time.sleep(.25)
    raise TimeoutError('Acceptance condition timed out')
def fetch(url,target,sha=None):
    data=urllib.request.urlopen(url,timeout=60).read()
    if sha and hashlib.sha256(data).hexdigest()!=sha:raise RuntimeError('Model SHA256 mismatch')
    target.write_bytes(data)
def mask(value):print('::add-mask::'+value,flush=True);return value
try:
    if sql("SHOW DATABASES LIKE 'nebulavision_dev'"):raise RuntimeError('Refusing to use an existing database; requires fresh CI MySQL')
    for migration in sorted((root/'NebulaVision/database/migrations').glob('*.sql')):sql(migration.read_text())
    db_password=mask(secrets.token_hex(24));rpc_secret=mask(secrets.token_hex(24));media_secret=mask(secrets.token_hex(24))
    sql("CREATE USER 'nv_ci'@'127.0.0.1' IDENTIFIED BY '%s'; GRANT SELECT,INSERT,UPDATE,DELETE ON nebulavision_dev.* TO 'nv_ci'@'127.0.0.1';"%db_password)
    accounts={}
    for role in ['admin','viewer']:
        username='ci_'+role;password=mask(secrets.token_hex(16));accounts[role]={'username':username,'password':password};salt=secrets.token_bytes(16);digest=hashlib.pbkdf2_hmac('sha256',password.encode(),salt,200000)
        stored='pbkdf2_sha256$200000$'+base64.b64encode(salt).decode()+'$'+base64.b64encode(digest).decode()
        sql("USE nebulavision_dev; INSERT INTO users(username,password_hash,status) VALUES ('%s','%s','active'); INSERT INTO user_roles(user_id,role_id) SELECT u.id,r.id FROM users u JOIN roles r ON r.code='%s' WHERE u.username='%s';"%(username,stored,role,username))
    (resources/'accounts.local.json').write_text(json.dumps(accounts));passed('fresh MySQL migrations and isolated accounts')
    manifest=json.loads((root/'detector/model_manifest.json').read_text());fetch(manifest['source'],resources/'models/yolov5n.onnx',manifest['sha256'])
    for filename,item in json.loads((root/'detector/live-model-manifest.json').read_text()).items():fetch(item['url'],resources/'models'/filename,item['sha256'])
    fetch('https://storage.googleapis.com/mediapipe-assets/right_hands.jpg',resources/'models/hands.jpg');fetch(manifest['sample_source'],resources/'models/person.jpg');passed('official model downloads verified by SHA256')
    ffmpeg=shutil.which('ffmpeg');clip=resources/'hands.mp4'
    run([ffmpeg,'-hide_banner','-loglevel','error','-loop','1','-i',resources/'models/hands.jpg','-t','40','-c:v','libx264','-tune','zerolatency','-g','25','-crf','10','-pix_fmt','yuv420p','-y',clip])
    model_version='yolov5n-v7-'+manifest['sha256'][:16]
    config={'database':'nebulavision_dev','username':'nv_ci','password':db_password,'port':18880,'redis_port':16379,'rate_per_minute':300,'media_secret':media_secret,'recording_root':str(resources/'recordings'),'ffmpeg':ffmpeg,'python':sys.executable,'python_packages':'','rpc_client':str(root/'detector/rpc_client.py'),'rpc_secret':rpc_secret,'detection_mode':'onnx','model_version':model_version}
    config_path=resources/'server.local.json';config_path.write_text(json.dumps(config))
    worker_config={'secret':rpc_secret,'state':str(resources/'jobs.sqlite3'),'allowed_roots':[str(resources)],'allow_test':False,'model':str(resources/'models/yolov5n.onnx'),'ffmpeg':ffmpeg};worker_path=resources/'detector.local.json';worker_path.write_text(json.dumps(worker_config))
    env=os.environ.copy();env.update({'NEBULAVISION_SERVER_CONFIG':str(config_path),'NEBULAVISION_PROJECT_ROOT':str(root),'NEBULAVISION_RESOURCES_ROOT':str(resources),'NEBULAVISION_AGENT_PYTHON':sys.executable,'NEBULAVISION_FFMPEG':ffmpeg,'NEBULAVISION_CACHE_ROOT':str(resources/'playback-cache'),'NEBULAVISION_TEST_OUTPUT':str(out),'PYTHONDONTWRITEBYTECODE':'1','QT_LOGGING_RULES':'qt.multimedia.ffmpeg.*=false'})
    spawn('redis',['redis-server','--bind','127.0.0.1','--port','16379','--save','','--appendonly','no'])
    worker=spawn('detector',[sys.executable,root/'detector/worker.py',worker_path],env)
    server=spawn('server',[build/'nebulavision_server'],env);wait(lambda:call('/health')[0]==200);passed('C++ backend startup and health')
    assert call('/api/cameras')[0]==401;assert call('/api/login',{'username':'ci_admin','password':'incorrect'})[0]==401
    admin=call('/api/login',accounts['admin'])[1]['token'];viewer=call('/api/login',accounts['viewer'])[1]['token'];assert call('/api/me',token=admin)[1]['admin']
    device={'name':'Linux CI public sample','sourceType':'local','sourceKey':hashlib.sha256(b'isolated-linux-public-sample').hexdigest()}
    assert call('/api/cameras/register',device,viewer)[0]==403;camera=call('/api/cameras/register',device,admin)[1]['id'];assert call('/api/cameras/register',device,admin)[1]['id']==camera;assert call('/api/cameras',token=viewer)[1]['cameras']==[];passed('login, invalid password, anonymous and viewer permission checks')
    result=run(['xvfb-run','-a',build/'linux_gui_acceptance',resources,camera,clip],env=env,capture_output=True,text=True,timeout=180)
    (out/'gui.log').write_text(result.stdout+'\n'+result.stderr)
    for marker in ['LINUX_MONITOR_FRAMES=PASS','LINUX_REAL_GESTURE_AND_CLIP=PASS','LINUX_IN_PAGE_REPLAY_SEEK_2X=PASS']:assert marker in result.stdout,marker
    passed('Qt GUI, actual FFmpeg frames, real MediaPipe gesture, automatic MP4 and event link');passed('in-page replay frames, pause/resume, seeking and 2x speed')
    event=next(e for e in call('/api/events?size=100',token=admin)[1]['events'] if e['cameraName']==device['name'] and e['type']=='gesture_open_palm');recording=event['recordingId'];assert int(recording)>0
    assert call('/api/recordings/file?id='+recording)[0]==401;assert call('/api/recordings/file?id='+recording,token=viewer)[0]==404
    code,data=call('/api/recordings/file?id='+recording,token=admin);assert code==200 and len(data)>1000
    sample=out/'evidence-check.mp4';sample.write_bytes(data);run([ffmpeg,'-hide_banner','-loglevel','error','-i',sample,'-frames:v','1','-f','null','-']);passed('authorized evidence download and actual decode; unauthorized download denied')
    wait(lambda:socket.create_connection(('127.0.0.1',18551),1).close() is None)
    rpc=run([sys.executable,root/'detector/rpc_client.py'],input=json.dumps({'jobId':'linux_person_check','path':str(resources/'models/person.jpg'),'mode':'onnx','threshold':.25,'roi':[],'secret':rpc_secret,'timeout':5}),text=True,capture_output=True,env=env)
    detected=json.loads(rpc.stdout);assert detected['synthetic'] is False and detected['modelVersion']==model_version and len(detected['detections'])>=1;passed('real YOLO person inference through Python gRPC adapter')
    code,job=call('/api/detections/start',{'recordingId':recording,'jobKey':'linux_'+uuid.uuid4().hex,'threshold':.25},admin);assert code==202
    wait(lambda:call('/api/detections/job?id='+job['id'],token=admin)[1]['status'] not in ['queued','running'])
    assert call('/api/detections/job?id='+job['id'],token=admin)[1]['status']=='succeeded';passed('C++ recording detection job through real gRPC service')
    sql("USE nebulavision_dev; INSERT INTO recordings(camera_id,started_at,object_key,status) VALUES (%s,UTC_TIMESTAMP(3),'ci-interrupted.mp4','writing');"%camera)
    server.terminate();server.wait(timeout=10);server=spawn('server',[build/'nebulavision_server'],env);wait(lambda:call('/health')[0]==200)
    assert sql("SELECT status FROM nebulavision_dev.recordings WHERE object_key='ci-interrupted.mp4'")=='interrupted';assert call('/api/me',token=admin)[0]==401;passed('backend restart, interrupted recording recovery and old session invalidation')
    report['success']=True
except Exception as error:
    report['success']=False;report['error']=type(error).__name__
    if isinstance(error,subprocess.CalledProcessError) and error.stdout:(out/'failed-command.log').write_text(error.stdout+'\n'+(error.stderr or ''))
    print('FAIL:',type(error).__name__,flush=True)
finally:
    for process in reversed(processes):
        if process.poll() is None:
            process.terminate()
            try:process.wait(timeout=8)
            except subprocess.TimeoutExpired:process.kill();process.wait()
    for log in logs:log.close()
    report['checks']['managed process cleanup']='PASS' if all(p.poll() is not None for p in processes) else 'FAIL'
    (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2));print(json.dumps(report,ensure_ascii=False,indent=2))
sys.exit(0 if report.get('success') else 1)
