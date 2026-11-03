#include "livepreview.h"
#include <QCamera>
#include <QMediaPlayer>
#include <QMediaCaptureSession>
#include <QVideoSink>
#include <QVideoFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QBuffer>
#include <QPainter>
#include <QDir>
#include <QCoreApplication>
#include <QCloseEvent>

LivePreview::LivePreview(QWidget *parent):QDialog(parent) {
    setAttribute(Qt::WA_DeleteOnClose);setWindowTitle("实时人员 人脸 手势检测");resize(860,650);
    auto layout=new QVBoxLayout(this);video=new QLabel("等待画面");video->setMinimumSize(480,320);video->setAlignment(Qt::AlignCenter);layout->addWidget(video,1);
    auto controls=new QHBoxLayout;threshold=new QDoubleSpinBox;threshold->setRange(.25,.95);threshold->setSingleStep(.05);threshold->setValue(.45);
    alarm=new QCheckBox("手势异常提示：张开手掌连续3次（置信度≥0.60）");alarm->setChecked(true);
    controls->addWidget(new QLabel("置信度"));controls->addWidget(threshold);controls->addWidget(alarm);
    auto stopButton=new QPushButton("停止");controls->addWidget(stopButton);layout->addLayout(controls);
    status=new QLabel("模型加载中……");status->setObjectName("live_status");status->setWordWrap(true);layout->addWidget(status);
    sink=new QVideoSink(this);
    connect(sink,&QVideoSink::videoFrameChanged,this,[this](const QVideoFrame &frame){
        if(!frame.isValid())return;
        latest=frame.toImage().scaled(960,540,Qt::KeepAspectRatio,Qt::FastTransformation);
        ++frameVersion;
    });
    sample.setInterval(350);connect(&sample,&QTimer::timeout,this,&LivePreview::sendFrame);
    deadline.setSingleShot(true);connect(&deadline,&QTimer::timeout,this,[this](){stop();status->setText("实时检测超时，已停止；请关闭窗口后重新打开。");});
    connect(&worker,&QProcess::readyReadStandardOutput,this,&LivePreview::receive);
    connect(&worker,&QProcess::readyReadStandardError,this,[this](){worker.readAllStandardError();});
    connect(&worker,&QProcess::errorOccurred,this,[this](QProcess::ProcessError){if(!sample.isActive())return;stop();status->setText("实时检测进程失败，请检查Python及模型。");});
    connect(&worker,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int,QProcess::ExitStatus){
        if(sample.isActive()){stop();status->setText("实时检测进程退出，请重新打开。");}
    });
    connect(stopButton,&QPushButton::clicked,this,[this](){stop();status->setText("已停止，重新打开摄像头可再次检测。");});
    connect(alarm,&QCheckBox::toggled,this,[this](){positive=negative=0;alarming=false;});
    connect(threshold,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this](){positive=negative=0;alarming=false;});
}
void LivePreview::startWorker(){
    QString root=QString::fromUtf8(qgetenv("NEBULAVISION_PROJECT_ROOT"));if(root.isEmpty())root=QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("..");
    QString resources=QString::fromUtf8(qgetenv("NEBULAVISION_RESOURCES_ROOT"));if(resources.isEmpty())resources=QDir(root).absoluteFilePath("../x");
    QString python=QString::fromUtf8(qgetenv("NEBULAVISION_AGENT_PYTHON"));if(python.isEmpty())python=QDir::homePath()+"/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe";
    worker.setProgram(python);worker.setArguments({root+"/detector/live_worker.py",resources});worker.start();sample.start();deadline.start(15000);
}
void LivePreview::openCamera(const QCameraDevice &device){
    camera=new QCamera(device,this);auto session=new QMediaCaptureSession(this);session->setCamera(camera);session->setVideoSink(sink);
    connect(camera,&QCamera::errorOccurred,this,[this](QCamera::Error,const QString &error){stop();status->setText("摄像头错误："+error);});
    startWorker();camera->start();
}
void LivePreview::openStream(const QString &url){
    QString source=QUrl(url).isLocalFile()?QUrl(url).toLocalFile():url;
    decodeTask=new DecodeTask(source,FfmpegPlayer::ffmpegExecutable());
    connect(decodeTask,&QThread::finished,decodeTask,&QObject::deleteLater);
    connect(decodeTask,&DecodeTask::failed,this,[this](const QString &error){stop();status->setText(error);});
    startWorker();decodeTask->start();
}
void LivePreview::sendFrame(){
    if(decodeTask){QImage frame;if(decodeTask->takeFrame(&frame)){latest=frame;++frameVersion;}}
    if(!ready || busy || latest.isNull() || sentVersion==frameVersion)return;
    sentVersion=frameVersion;
    pending=latest;QByteArray jpeg;QBuffer output(&jpeg);output.open(QIODevice::WriteOnly);pending.save(&output,"JPEG",85);
    quint32 size=quint32(jpeg.size());QByteArray packet;for(int shift=24;shift>=0;shift-=8)packet.append(char((size>>shift)&255));packet.append(jpeg);
    busy=true;deadline.start(7000);worker.write(packet);
}
void LivePreview::receive(){
    if(!sample.isActive()){worker.readAllStandardOutput();buffer.clear();return;}
    buffer+=worker.readAllStandardOutput();
    while(buffer.contains('\n')){
        int end=buffer.indexOf('\n');auto data=QJsonDocument::fromJson(buffer.left(end)).object();buffer.remove(0,end+1);
        if(data.contains("error")){stop();status->setText(data.value("error").toString());return;}
        if(data.value("ready").toBool()){ready=true;status->setText("模型已加载，等待摄像头画面……");continue;}
        if(!busy || pending.isNull())continue;
        QImage image=pending;QPainter painter(&image);int people=0;painter.setPen(QPen(Qt::green,3));
        for(const auto &value:data.value("detections").toArray()){
            auto box=value.toObject();double score=box.value("confidence").toDouble();if(score<threshold->value())continue;++people;
            QRectF rect(box.value("x").toDouble()*image.width(),box.value("y").toDouble()*image.height(),box.value("width").toDouble()*image.width(),box.value("height").toDouble()*image.height());
            painter.drawRect(rect);painter.drawText(rect.topLeft()+QPointF(0,18),QString("person %1").arg(score,0,'f',2));
        }
        auto faces=data.value("faces").toArray();auto hands=data.value("hands").toArray();bool gestureAlarm=false;
        painter.setPen(QPen(Qt::cyan,3));
        for(const auto &value:faces){auto box=value.toObject();QRectF rect(box.value("x").toDouble()*image.width(),box.value("y").toDouble()*image.height(),box.value("width").toDouble()*image.width(),box.value("height").toDouble()*image.height());painter.drawRect(rect);painter.drawText(rect.topLeft()+QPointF(0,18),"face");}
        painter.setPen(QPen(QColor(255,160,0),3));
        for(const auto &value:hands){auto box=value.toObject();QString name=box.value("gesture").toString();double score=box.value("confidence").toDouble();
            QRectF rect(box.value("x").toDouble()*image.width(),box.value("y").toDouble()*image.height(),box.value("width").toDouble()*image.width(),box.value("height").toDouble()*image.height());painter.drawRect(rect);painter.drawText(rect.topLeft()+QPointF(0,18),name+QString(" %1").arg(score,0,'f',2));
            if(name=="Open_Palm" && score>=.6)gestureAlarm=true;
        }
        painter.end();++processedFrames;lastPeople=people;
        if(gestureAlarm){++positive;negative=0;}else{positive=0;++negative;}
        if(alarm->isChecked() && positive>=3)alarming=true;if(negative>=3 || !alarm->isChecked())alarming=false;
        lastFaces=int(faces.size());gestureAlert=alarming;
        status->setStyleSheet(alarming?"color:#d22;font-weight:bold":"");
        status->setText(QString("%1 人员 %2，人脸 %3，手 %4，推理 %5 ms，已检测 %6 帧。仅本窗口提示，不自动入库或录像。")
            .arg(alarming?"手势异常：张开手掌！":"持续采样检测中").arg(people).arg(faces.size()).arg(hands.size()).arg(data.value("elapsedMs").toInt()).arg(processedFrames));
        video->setPixmap(QPixmap::fromImage(image).scaled(video->size(),Qt::KeepAspectRatio,Qt::SmoothTransformation));
        busy=false;pending=QImage();deadline.start(10000);
    }
}
void LivePreview::stop(){sample.stop();deadline.stop();ready=false;busy=false;gestureAlert=false;positive=negative=0;if(camera)camera->stop();if(decodeTask){decodeTask->requestStop();decodeTask=nullptr;}if(worker.state()!=QProcess::NotRunning)worker.kill();latest=QImage();pending=QImage();status->setStyleSheet("");status->setText("已停止，关闭窗口后重新打开可再次检测。");}
void LivePreview::closeEvent(QCloseEvent *event){stop();worker.waitForFinished(1000);event->accept();}
