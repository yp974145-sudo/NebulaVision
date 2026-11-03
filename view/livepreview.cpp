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
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QUuid>
#include "../service/apiclient.h"
#include "../service/alertcliprecorder.h"

LivePreview::LivePreview(QWidget *parent):QWidget(parent) {
    setAttribute(Qt::WA_DeleteOnClose);setWindowTitle("实时人员 人脸 手势检测");resize(860,650);
    auto layout=new QVBoxLayout(this);layout->setContentsMargins(4,4,4,4);video=new QLabel("等待画面");video->setMinimumSize(160,90);video->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Ignored);video->setAlignment(Qt::AlignCenter);layout->addWidget(video,1);
    auto controls=new QHBoxLayout;threshold=new QDoubleSpinBox;threshold->setRange(.25,.95);threshold->setSingleStep(.05);threshold->setValue(.45);
    alarm=new QCheckBox("手势告警");alarm->setToolTip("张开手掌分数达到0.60，连续3次触发，连续3次不满足清除");alarm->setChecked(true);
    controls->addWidget(new QLabel("置信度"));controls->addWidget(threshold);controls->addWidget(alarm);
    auto stopButton=new QPushButton("停止");controls->addWidget(stopButton);layout->addLayout(controls);
    status=new QLabel("模型加载中……");status->setObjectName("live_status");status->setWordWrap(true);layout->addWidget(status);
    savedStatus=new QLabel("异常事件等待触发");savedStatus->setObjectName("event_save_status");savedStatus->setWordWrap(true);layout->addWidget(savedStatus);
    retrySave=new QPushButton("重试入库");retrySave->hide();layout->addWidget(retrySave);connect(retrySave,&QPushButton::clicked,this,&LivePreview::saveAlert);
    clipRecorder=new AlertClipRecorder(this);
    auto clipStatus=new QLabel("异常时自动保存短片：报警前最多3秒＋报警后5秒");clipStatus->setObjectName("clip_status");clipStatus->setWordWrap(true);layout->addWidget(clipStatus);
    auto retryClip=new QPushButton("重试录像");layout->addWidget(retryClip);retryClip->hide();
    connect(clipRecorder,&AlertClipRecorder::statusChanged,this,[clipStatus,retryClip](const QString &text){clipStatus->setText(text);retryClip->setVisible(text.contains("失败") || text.contains("先重试"));});
    connect(retryClip,&QPushButton::clicked,clipRecorder,&AlertClipRecorder::retry);
    clipSample.setInterval(200);connect(&clipSample,&QTimer::timeout,this,[this](){if(!paused && sample.isActive())clipRecorder->sample(latest);});clipSample.start();
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
    worker.waitForFinished(1000);ready=false;busy=false;paused=false;buffer.clear();latest=QImage();pending=QImage();frameVersion=sentVersion=0;
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
    lastUrl=url;
    QString source=QUrl(url).isLocalFile()?QUrl(url).toLocalFile():url;
    decodeTask=new DecodeTask(source,FfmpegPlayer::ffmpegExecutable());
    connect(decodeTask,&QThread::finished,decodeTask,&QObject::deleteLater);
    connect(decodeTask,&DecodeTask::failed,this,[this](const QString &error){stop();status->setText(error);});
    startWorker();decodeTask->start();
}
void LivePreview::sendFrame(){
    if(paused)return;
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
        if(paused){busy=false;pending=QImage();continue;}
        QImage image=pending;QPainter painter(&image);int people=0;painter.setPen(QPen(Qt::green,3));
        for(const auto &value:data.value("detections").toArray()){
            auto box=value.toObject();double score=box.value("confidence").toDouble();if(score<threshold->value())continue;++people;
            QRectF rect(box.value("x").toDouble()*image.width(),box.value("y").toDouble()*image.height(),box.value("width").toDouble()*image.width(),box.value("height").toDouble()*image.height());
            painter.drawRect(rect);painter.drawText(rect.topLeft()+QPointF(0,18),QString("person %1").arg(score,0,'f',2));
        }
        auto faces=data.value("faces").toArray();auto hands=data.value("hands").toArray();bool gestureAlarm=false;lastGestureScore=0;
        painter.setPen(QPen(Qt::cyan,3));
        for(const auto &value:faces){auto box=value.toObject();QRectF rect(box.value("x").toDouble()*image.width(),box.value("y").toDouble()*image.height(),box.value("width").toDouble()*image.width(),box.value("height").toDouble()*image.height());painter.drawRect(rect);painter.drawText(rect.topLeft()+QPointF(0,18),"face");}
        painter.setPen(QPen(QColor(255,160,0),3));
        for(const auto &value:hands){auto box=value.toObject();QString name=box.value("gesture").toString();double score=box.value("confidence").toDouble();
            QRectF rect(box.value("x").toDouble()*image.width(),box.value("y").toDouble()*image.height(),box.value("width").toDouble()*image.width(),box.value("height").toDouble()*image.height());painter.drawRect(rect);painter.drawText(rect.topLeft()+QPointF(0,18),name+QString(" %1").arg(score,0,'f',2));
            if(name=="Open_Palm"){lastGestureScore=qMax(lastGestureScore,score);if(score>=.6)gestureAlarm=true;}
        }
        painter.end();++processedFrames;lastPeople=people;
        if(gestureAlarm){++positive;negative=0;}else{positive=0;++negative;}
        bool wasAlarming=alarming;
        if(alarm->isChecked() && positive>=3)alarming=true;if(negative>=3 || !alarm->isChecked())alarming=false;
        if(alarming && !wasAlarming && !saving && pendingAlert.isEmpty()){
            double confidence=.6;for(const auto &value:hands){auto hand=value.toObject();if(hand.value("gesture").toString()=="Open_Palm")confidence=qMax(confidence,hand.value("confidence").toDouble());}
            pendingAlert={{"cameraId",cameraId},{"eventKey",QUuid::createUuid().toString(QUuid::WithoutBraces)},{"gesture","Open_Palm"},{"confidence",confidence},{"consecutive",3}};
            if(!cameraId.isEmpty())clipRecorder->begin(pendingAlert.value("eventKey").toString());
            saveAlert();
        }
        lastFaces=int(faces.size());gestureAlert=alarming;
        status->setStyleSheet(alarming?"color:#d22;font-weight:bold":"");
        status->setText(QString("%1 人 %2，脸 %3，手 %4，%5 ms，第 %6 帧；张手 %7，连续 %8/3")
            .arg(alarming?"手势异常！":"实时检测").arg(people).arg(faces.size()).arg(hands.size()).arg(data.value("elapsedMs").toInt()).arg(processedFrames).arg(lastGestureScore,0,'f',3).arg(qMin(positive,3)));
        video->setPixmap(QPixmap::fromImage(image).scaled(video->size(),Qt::KeepAspectRatio,Qt::SmoothTransformation));
        busy=false;pending=QImage();deadline.start(10000);
    }
}
void LivePreview::stop(){clipRecorder->finish();clipRecorder->resetHistory();sample.stop();deadline.stop();ready=false;busy=false;gestureAlert=false;positive=negative=0;if(camera)camera->stop();if(decodeTask){decodeTask->requestStop();decodeTask=nullptr;}if(worker.state()!=QProcess::NotRunning)worker.kill();latest=QImage();pending=QImage();status->setStyleSheet("");status->setText("已停止，关闭窗口后重新打开可再次检测。");}
void LivePreview::closeEvent(QCloseEvent *event){stop();worker.waitForFinished(1000);event->accept();}
LivePreview::~LivePreview(){stop();worker.waitForFinished(1000);}
void LivePreview::pause(){clipRecorder->finish();clipRecorder->resetHistory();paused=true;deadline.stop();status->setText("已暂停");}
void LivePreview::resume(){if(sample.isActive()){paused=false;deadline.start(15000);return;}if(camera){startWorker();camera->start();}else if(!lastUrl.isEmpty())openStream(lastUrl);}
void LivePreview::setCameraId(const QString &id){cameraId=id;}
// 同一条告警保留同一个eventKey。网络失败后重试，后台不会重复插入。
void LivePreview::saveAlert(){
    if(saving || pendingAlert.isEmpty())return;
    if(cameraId.isEmpty()){savedStatus->setText("设备尚未入库，请管理员刷新设备后重新打开。");return;}
    saving=true;retrySave->hide();savedStatus->setText("手势异常入库中……");auto body=pendingAlert;
    struct Result{QJsonObject data;QString error;};auto watcher=new QFutureWatcher<Result>(this);
    connect(watcher,&QFutureWatcher<Result>::finished,this,[this,watcher](){auto r=watcher->result();watcher->deleteLater();saving=false;
        if(!r.error.isEmpty()){savedStatus->setText("入库失败："+r.error);retrySave->show();return;}
        lastSavedEvent=r.data.value("id").toString();pendingAlert={};savedStatus->setText("事件已入库 #"+lastSavedEvent+"，到事件列表查看。");
    });
    // body按值复制给工作线程，窗口关闭时不会让线程引用已经销毁的界面对象。
    watcher->setFuture(QtConcurrent::run([body](){Result r;r.data=ApiClient::request("/api/events/gesture",body,true,&r.error);return r;}));
}
