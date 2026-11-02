"""C++的本机gRPC适配器。输入走stdin，不将密钥放到进程参数或日志。"""
import sys,json
import grpc
import detection_pb2 as pb
import detection_pb2_grpc as rpc
request=json.load(sys.stdin)
try:
 with grpc.insecure_channel('127.0.0.1:18551') as channel:
  reply=rpc.DetectorStub(channel).Detect(pb.DetectRequest(job_id=request['jobId'],media_path=request['path'],mode=request['mode'],threshold=request.get('threshold',.25),roi=request.get('roi',[])),timeout=request.get('timeout',5),metadata=[('authorization','Bearer '+request['secret'])])
  data={'jobId':reply.job_id,'synthetic':reply.synthetic,'modelVersion':reply.model_version,'cacheHit':reply.cache_hit,'detections':[{'classId':d.class_id,'label':d.label,'confidence':d.confidence,'x':d.x,'y':d.y,'width':d.width,'height':d.height} for d in reply.detections]}
  print(json.dumps(data))
except grpc.RpcError as e:
 print(json.dumps({'error':e.code().name}));sys.exit(2)
