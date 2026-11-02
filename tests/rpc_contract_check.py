import json,sys,uuid,time
from pathlib import Path
import grpc
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'detector'))
import detection_pb2 as pb
import detection_pb2_grpc as rpc
x=Path(sys.argv[1]);config=json.loads((x.parent/'class15_backup_20261102/NebulaVision/config/detector.local.json').read_text(encoding='utf-8-sig'))
with grpc.insecure_channel('127.0.0.1:18551') as channel:
 grpc.channel_ready_future(channel).result(timeout=5);stub=rpc.DetectorStub(channel);metadata=[('authorization','Bearer '+config['secret'])]
 request=pb.DetectRequest(job_id='contract_'+uuid.uuid4().hex,media_path=str(x/'models/zidane.jpg'),mode='test',threshold=.25)
 first=stub.Detect(request,timeout=2,metadata=metadata);second=stub.Detect(request,timeout=2,metadata=metadata)
 assert first.synthetic and second.cache_hit
 try:stub.Detect(request,timeout=2);raise AssertionError('missing RPC auth accepted')
 except grpc.RpcError as e:assert e.code()==grpc.StatusCode.UNAUTHENTICATED
 request.threshold=.4
 try:stub.Detect(request,timeout=2,metadata=metadata);raise AssertionError('changed payload accepted')
 except grpc.RpcError as e:assert e.code()==grpc.StatusCode.ALREADY_EXISTS
 delayed=pb.DetectRequest(job_id='deadline_'+uuid.uuid4().hex,media_path=str(x/'models/zidane.jpg'),mode='test',threshold=.25,test_delay_ms=1000)
 try:stub.Detect(delayed,timeout=.05,metadata=metadata);raise AssertionError('deadline ignored')
 except grpc.RpcError as e:assert e.code()==grpc.StatusCode.DEADLINE_EXCEEDED
 delayed.job_id='cancel_'+uuid.uuid4().hex
 future=stub.Detect.future(delayed,timeout=2,metadata=metadata);time.sleep(.03);assert future.cancel()
 try:future.result();raise AssertionError('cancel ignored')
 except grpc.FutureCancelledError:pass
 print('PASS: Protobuf contract, RPC authentication, cached repeat, changed payload rejected, deadline, cancellation')
