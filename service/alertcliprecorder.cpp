#include "alertcliprecorder.h"
#include "apiclient.h"
#include "../player/ffmpegplayer.h"
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QCoreApplication>
#include <QPainter>
void AlertClipRecorder::sample(const QImage &frame){
    if(frame.isNull())return;
    // 编码尺寸固定，4:3摄像头加黑边，避免强行拉伸人物比例。
    QImage fitted=frame.scaled(640,360,Qt::KeepAspectRatio,Qt::SmoothTransformation);
    QImage image(640,360,QImage::Format_RGB888);image.fill(Qt::black);
    {QPainter painter(&image);painter.drawImage((640-fitted.width())/2,(360-fitted.height())/2,fitted);}
    history.append(image);if(history.size()>15)history.removeFirst();
    if(collecting){frames.append(image);if(++after>=25)finish();}
}
void AlertClipRecorder::begin(const QString &eventKey){
    if(collecting || busy || !path.isEmpty()){emit statusChanged("上一段异常录像尚未保存，请先重试录像");return;}
    key=eventKey;token=ApiSession::token();frames=history;before=frames.size();after=0;collecting=true;
    emit statusChanged("异常录像采集中：保留报警前最多3秒，再录5秒……");
}
void AlertClipRecorder::finish(){
    if(!collecting)return;collecting=false;
    if(frames.size()<5){frames.clear();emit statusChanged("录像不足1秒，未保存异常录像");return;}
    QString root=QString::fromUtf8(qgetenv("NEBULAVISION_RESOURCES_ROOT"));if(root.isEmpty())root=QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("../../x");
    QDir directory(root+"/alert-cache");directory.mkpath(".");path=directory.filePath(key+".mp4");upload();
}
void AlertClipRecorder::retry(){if(!collecting && !busy && !path.isEmpty())upload();}
void AlertClipRecorder::upload(){
    busy=true;emit statusChanged("异常录像正在封片并上传……");
    // 复制帧、令牌和路径。控件销毁后，已开始的封片上传仍可自行结束。
    auto images=frames;QString destination=path,eventKey=key,session=token,ffmpeg=FfmpegPlayer::ffmpegExecutable();int pre=before;
    struct Result{QString error,id;};auto watcher=new QFutureWatcher<Result>(this);
    connect(watcher,&QFutureWatcher<Result>::finished,this,[this,watcher](){auto r=watcher->result();watcher->deleteLater();busy=false;
        if(!r.error.isEmpty()){emit statusChanged("异常录像保存失败："+r.error+"；可点重试录像");return;}
        path.clear();frames.clear();emit statusChanged("异常录像已保存 #"+r.id+"，到回放页双击对应事件");
    });
    watcher->setFuture(QtConcurrent::run([images,destination,eventKey,session,ffmpeg,pre](){
        Result r;
        if(!QFile::exists(destination)){
            QProcess encoder;encoder.setStandardOutputFile(QProcess::nullDevice());encoder.setStandardErrorFile(QProcess::nullDevice());
            encoder.start(ffmpeg,{"-hide_banner","-loglevel","error","-y","-f","rawvideo","-pix_fmt","rgb24","-s","640x360","-r","5","-i","pipe:0","-an","-c:v","libx264","-preset","veryfast","-pix_fmt","yuv420p","-movflags","+faststart",destination});
            if(!encoder.waitForStarted(3000)){r.error="编码器启动失败";return r;}
            for(const auto &image:images){
                QByteArray bytes(reinterpret_cast<const char *>(image.constBits()),image.sizeInBytes());
                if(encoder.write(bytes)!=bytes.size()){r.error="编码写入失败";break;}
                while(encoder.bytesToWrite()>0)if(!encoder.waitForBytesWritten(3000)){r.error="编码写入超时";break;}
                if(!r.error.isEmpty())break;
            }
            encoder.closeWriteChannel();bool done=encoder.waitForFinished(10000);
            if(!done){encoder.kill();encoder.waitForFinished(1000);}
            if(!r.error.isEmpty() || !done || encoder.exitCode()!=0){QFile::remove(destination);if(r.error.isEmpty())r.error="录像编码失败";return r;}
        }
        QFile file(destination);if(!file.open(QIODevice::ReadOnly) || file.size()>12*1024*1024){r.error="录像不可读或超过12MB";return r;}
        QJsonObject body{{"eventKey",eventKey},{"preFrames",pre},{"frames",int(images.size())},{"video",QString::fromLatin1(file.readAll().toBase64())}};file.close();
        auto result=ApiClient::request("/api/events/clip",body,true,&r.error,session);r.id=result.value("recordingId").toString();
        if(r.error.isEmpty() && r.id.toLongLong()>0)QFile::remove(destination);
        else if(r.error.isEmpty())r.error="服务端未返回录像编号";
        return r;
    }));
}
