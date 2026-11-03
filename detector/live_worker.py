"""常驻实时推理：stdin为4字节大端长度+JPEG，stdout每行一个JSON结果。"""
import sys
sys.dont_write_bytecode=True
import json,struct,time,os
from pathlib import Path
resource=Path(sys.argv[1]).resolve()
sys.path.insert(0,str(resource/'live-python'))
os.environ.setdefault('MPLCONFIGDIR',str(resource/'detector-state/matplotlib-cache'))
import cv2,numpy as np
import mediapipe as mp
from inference import Engine

def send(value):
    print(json.dumps(value,ensure_ascii=True),flush=True)
def read_exact(size):
    data=bytearray()
    while len(data)<size:
        part=sys.stdin.buffer.read(size-len(data))
        if not part:return None
        data.extend(part)
    return data
try:
    engine=Engine({'model':str(resource/'models/yolov5n.onnx')})
    face_model=cv2.CascadeClassifier(str(resource/'models/haarcascade_frontalface_default.xml'))
    if face_model.empty():raise RuntimeError('face model missing')
    options=mp.tasks.vision.GestureRecognizerOptions(
        base_options=mp.tasks.BaseOptions(model_asset_path=str(resource/'models/gesture_recognizer.task')),
        running_mode=mp.tasks.vision.RunningMode.IMAGE,num_hands=2)
    gesture=mp.tasks.vision.GestureRecognizer.create_from_options(options)
    send({'ready':True,'model':engine.model_version})
    while True:
        header=read_exact(4)
        if header is None:break
        size=struct.unpack('>I',header)[0]
        if size<1 or size>2*1024*1024:raise ValueError('invalid frame length')
        raw=read_exact(size)
        if raw is None:break
        frame=cv2.imdecode(np.frombuffer(raw,np.uint8),cv2.IMREAD_COLOR)
        begin=time.monotonic();boxes=engine.detect_frame(frame,.25)
        height,width=frame.shape[:2]
        gray=cv2.cvtColor(frame,cv2.COLOR_BGR2GRAY)
        faces=[{'x':int(x)/width,'y':int(y)/height,'width':int(w)/width,'height':int(h)/height}
            for x,y,w,h in face_model.detectMultiScale(gray,scaleFactor=1.1,minNeighbors=5,minSize=(35,35))]
        rgb=cv2.cvtColor(frame,cv2.COLOR_BGR2RGB)
        recognition=gesture.recognize(mp.Image(image_format=mp.ImageFormat.SRGB,data=np.ascontiguousarray(rgb)))
        hands=[]
        for categories,landmarks in zip(recognition.gestures,recognition.hand_landmarks):
            category=categories[0] if categories else None
            x=max(0.,min(point.x for point in landmarks)-.02);y=max(0.,min(point.y for point in landmarks)-.02)
            x2=min(1.,max(point.x for point in landmarks)+.02);y2=min(1.,max(point.y for point in landmarks)+.02)
            hands.append({'gesture':category.category_name if category else 'None','confidence':float(category.score) if category else 0.,
                          'x':x,'y':y,'width':max(0.,x2-x),'height':max(0.,y2-y)})
        send({'detections':boxes,'faces':faces,'hands':hands,'elapsedMs':round((time.monotonic()-begin)*1000)})
except Exception as error:
    send({'error':'实时模型运行失败，请检查模型和依赖。','type':type(error).__name__})
    sys.exit(1)
