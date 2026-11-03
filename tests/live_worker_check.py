"""传入x及任务样本目录（right_hands.jpg、lena.jpg），真实模型协议检查。"""
import sys
sys.dont_write_bytecode=True
import json,struct,subprocess
from pathlib import Path
x,samples=map(Path,sys.argv[1:3]);root=Path(__file__).resolve().parents[1]
packet=b''
for name in ['right_hands.jpg','lena.jpg']:
    raw=(samples/name).read_bytes();packet+=struct.pack('>I',len(raw))+raw
result=subprocess.run([sys.executable,str(root/'detector/live_worker.py'),str(x)],input=packet,capture_output=True,timeout=25)
assert result.returncode==0
responses=[json.loads(line) for line in result.stdout.splitlines()]
assert responses[0]['ready'] and len(responses)==3
assert any(h['gesture']=='Open_Palm' and h['confidence']>=.6 for h in responses[1]['hands'])
assert len(responses[2]['faces'])>=1
for response in responses[1:]:
    assert response['elapsedMs']>=0
    for field in ('detections','faces','hands'):
        for box in response[field]:
            assert 0<=box['x']<=1 and 0<=box['y']<=1 and 0<=box['width']<=1 and 0<=box['height']<=1
bad=subprocess.run([sys.executable,str(root/'detector/live_worker.py'),str(x)],input=struct.pack('>I',3*1024*1024),capture_output=True,timeout=25)
assert bad.returncode!=0 and json.loads(bad.stdout.splitlines()[-1])['error']
print('PASS: real palm recognition, face detection, bounded JPEG protocol, normalized boxes, invalid packet rejection')
