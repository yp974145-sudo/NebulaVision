"""独立gRPC检测进程。仅监听本机，校验元数据密钥、输入根目录及截止时间。"""
import sys,json,sqlite3,hashlib,threading,time,secrets,re,subprocess
sys.dont_write_bytecode=True
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import grpc
import detection_pb2 as pb
import detection_pb2_grpc as rpc

config=json.loads(Path(sys.argv[1]).read_text(encoding='utf-8-sig'))
lock=threading.Lock();slots=threading.BoundedSemaphore(2)
from inference import Engine
engine=Engine(config)
state=Path(config['state']);state.parent.mkdir(parents=True,exist_ok=True)
with sqlite3.connect(state) as db:
 db.execute('CREATE TABLE IF NOT EXISTS jobs(job_id TEXT PRIMARY KEY,fingerprint TEXT,state TEXT,response BLOB)')
 db.execute("DELETE FROM jobs WHERE state='running'")

class Detector(rpc.DetectorServicer):
 def Detect(self,request,context):
  credentials=dict(context.invocation_metadata()).get('authorization','')
  if not secrets.compare_digest(credentials,'Bearer '+config['secret']):context.abort(grpc.StatusCode.UNAUTHENTICATED,'RPC authentication required')
  if not re.fullmatch(r'[A-Za-z0-9_-]{1,80}',request.job_id):context.abort(grpc.StatusCode.INVALID_ARGUMENT,'invalid job id')
  path=Path(request.media_path).resolve()
  roots=[Path(p).resolve() for p in config['allowed_roots']]
  if not path.is_file() or not any(path.is_relative_to(root) for root in roots):context.abort(grpc.StatusCode.PERMISSION_DENIED,'media path forbidden')
  if request.mode=='test' and not config.get('allow_test',False):context.abort(grpc.StatusCode.PERMISSION_DENIED,'test mode disabled')
  if request.mode not in ('test','onnx'):context.abort(grpc.StatusCode.INVALID_ARGUMENT,'invalid detector mode')
  if not 0.05<=request.threshold<=0.95:context.abort(grpc.StatusCode.INVALID_ARGUMENT,'invalid threshold')
  if request.roi and (len(request.roi)!=4 or not 0<=request.roi[0]<request.roi[2]<=1 or not 0<=request.roi[1]<request.roi[3]<=1):context.abort(grpc.StatusCode.INVALID_ARGUMENT,'invalid ROI')
  fingerprint=hashlib.sha256(request.SerializeToString()+(engine.model_version if request.mode=='onnx' else 'synthetic-contract-v1').encode()).hexdigest()
  with lock,sqlite3.connect(state) as db:
   old=db.execute('SELECT fingerprint,state,response FROM jobs WHERE job_id=?',(request.job_id,)).fetchone()
   if old:
    if old[0]!=fingerprint:context.abort(grpc.StatusCode.ALREADY_EXISTS,'job payload changed')
    if old[1]=='done':
     reply=pb.DetectReply.FromString(old[2]);reply.cache_hit=True;return reply
    context.abort(grpc.StatusCode.ABORTED,'job is running')
   db.execute("INSERT INTO jobs VALUES (?,?,'running',NULL)",(request.job_id,fingerprint))
  acquired=slots.acquire(blocking=False);stored=False
  try:
   if not acquired:context.abort(grpc.StatusCode.RESOURCE_EXHAUSTED,'detector busy')
   if request.mode=='test':
    deadline=time.monotonic()+min(request.test_delay_ms,5000)/1000
    while time.monotonic()<deadline:
     if not context.is_active():context.abort(grpc.StatusCode.CANCELLED,'cancelled')
     time.sleep(.01)
    reply=pb.DetectReply(job_id=request.job_id,synthetic=True,model_version='synthetic-contract-v1')
    reply.detections.add(class_id=0,label='person',confidence=.9,x=.1,y=.1,width=.2,height=.3)
   else:
    try:
     detections=engine.detect(path,request.threshold,request.roi,context.time_remaining() or 5)
    except Exception:
     context.abort(grpc.StatusCode.UNAVAILABLE,'inference or frame extraction failed')
    reply=pb.DetectReply(job_id=request.job_id,synthetic=False,model_version=engine.model_version)
    for detection in detections:reply.detections.add(**detection)
   if not context.is_active():context.abort(grpc.StatusCode.CANCELLED,'cancelled')
   with lock,sqlite3.connect(state) as db:db.execute("UPDATE jobs SET state='done',response=? WHERE job_id=?",(reply.SerializeToString(),request.job_id))
   stored=True;return reply
  finally:
   if acquired:slots.release()
   if not stored:
    with lock,sqlite3.connect(state) as db:db.execute("DELETE FROM jobs WHERE job_id=? AND state='running'",(request.job_id,))

server=grpc.server(ThreadPoolExecutor(max_workers=4),options=[('grpc.max_receive_message_length',1048576),('grpc.max_send_message_length',1048576)])
rpc.add_DetectorServicer_to_server(Detector(),server)
if not server.add_insecure_port('127.0.0.1:18551'):raise RuntimeError('RPC port unavailable')
server.start()
try:server.wait_for_termination()
except KeyboardInterrupt:server.stop(2)
