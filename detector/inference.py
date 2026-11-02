"""YOLOv5n ONNX CPU推理：固定模型、FFmpeg采样、OpenCV前处理、NMS和区域规则。"""
import hashlib,subprocess,time
from pathlib import Path
import cv2,numpy as np,onnxruntime as ort

class Engine:
 def __init__(self,config):
  self.config=config
  self.model_version='yolov5n-v7-'+hashlib.sha256(Path(config['model']).read_bytes()).hexdigest()[:16]
  options=ort.SessionOptions();options.intra_op_num_threads=2;options.inter_op_num_threads=1
  self.session=ort.InferenceSession(config['model'],sess_options=options,providers=['CPUExecutionProvider'])
 def detect(self,path,threshold,roi,remaining):
  if path.suffix.lower() in ('.jpg','.jpeg','.png'):
   frame=cv2.imdecode(np.frombuffer(path.read_bytes(),np.uint8),cv2.IMREAD_COLOR)
  else:
   result=subprocess.run([self.config['ffmpeg'],'-hide_banner','-loglevel','error','-ss','0.3','-i',str(path),'-frames:v','1','-vf','scale=1280:720:force_original_aspect_ratio=decrease','-f','image2pipe','-vcodec','png','pipe:1'],capture_output=True,timeout=max(.1,min(3.5,remaining-.1)))
   if result.returncode or not result.stdout or len(result.stdout)>8*1024*1024:raise RuntimeError('frame extraction failed')
   frame=cv2.imdecode(np.frombuffer(result.stdout,np.uint8),cv2.IMREAD_COLOR)
  if frame is None:raise RuntimeError('invalid frame')
  height,width=frame.shape[:2];gain=min(640/width,640/height);nw,nh=round(width*gain),round(height*gain)
  left,top=(640-nw)//2,(640-nh)//2
  resized=cv2.resize(frame,(nw,nh),interpolation=cv2.INTER_LINEAR)
  padded=cv2.copyMakeBorder(resized,top,640-nh-top,left,640-nw-left,cv2.BORDER_CONSTANT,value=(114,114,114))
  rgb=cv2.cvtColor(padded,cv2.COLOR_BGR2RGB)
  tensor=np.ascontiguousarray(rgb.transpose(2,0,1)[None],dtype=np.float32)/255.0
  # 官方模型可能导出为FP16；按模型输入类型转换，不能固定假设float32。
  dtype=np.float16 if self.session.get_inputs()[0].type=="tensor(float16)" else np.float32
  tensor=np.ascontiguousarray(tensor,dtype=dtype)
  prediction=self.session.run(None,{self.session.get_inputs()[0].name:tensor})[0][0]
  # YOLOv5输出：中心x/y、宽高、objectness、80个类别分数；只做person规则。
  scores=prediction[:,4]*prediction[:,5]
  valid=np.isfinite(prediction[:,:6]).all(axis=1)&(scores>=threshold)
  boxes=[];confidences=[]
  for row,score in zip(prediction[valid],scores[valid]):
   x1=max(0.,min(float(width),(float(row[0])-float(row[2])/2-left)/gain));y1=max(0.,min(float(height),(float(row[1])-float(row[3])/2-top)/gain))
   x2=max(0.,min(float(width),(float(row[0])+float(row[2])/2-left)/gain));y2=max(0.,min(float(height),(float(row[1])+float(row[3])/2-top)/gain))
   if x2<=x1 or y2<=y1:continue
   boxes.append([x1,y1,x2-x1,y2-y1]);confidences.append(float(score))
  indices=cv2.dnn.NMSBoxes(boxes,confidences,threshold,.45)
  region=list(roi) if roi else [0.,0.,1.,1.];detections=[]
  for index in np.asarray(indices).reshape(-1):
   x,y,w,h=boxes[int(index)];cx=(x+w/2)/width;cy=(y+h/2)/height
   if region[0]<=cx<=region[2] and region[1]<=cy<=region[3]:
    detections.append({'class_id':0,'label':'person','confidence':min(1.,confidences[int(index)]),'x':x/width,'y':y/height,'width':w/width,'height':h/height})
  return detections
