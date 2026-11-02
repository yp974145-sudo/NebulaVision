#include <QApplication>
#include <QElapsedTimer>
#include <QThread>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include "view/monitorwidget.h"
void spin8(int ms){QElapsedTimer t;t.start();while(t.elapsed()<ms){QApplication::processEvents();QThread::msleep(10);}}
int main(int argc,char **argv) {
    QApplication app(argc,argv);
    MonitorWidget *widget=new MonitorWidget;
    widget->resize(1450,850);widget->setAttribute(Qt::WA_DontShowOnScreen);widget->show();
    widget->switch_to_channel4();
    int frames[4]={0,0,0,0};
    FfmpegPlayer *players[4];
    for(int i=0;i<4;i++) {
        players[i]=widget->findChild<FfmpegPlayer *>(QString("previewPlayer%1").arg(i));
        if(!players[i]) return 1;
        QObject::connect(players[i],&FfmpegPlayer::frameDisplayed,[&frames,i](){++frames[i];});
        widget->openSource(i,app.arguments().at(1));
    }
    int result=[&]()->int {
        spin8(5500);
        for(int i=0;i<4;i++) if(frames[i]<10)return 2;
        // 故意阻塞GUI 1秒，后台继续解码；最新帧缓存仍不能超过1。
        QThread::msleep(1000);
        int totalDropped=0;
        for(int i=0;i<4;i++) {
            if(players[i]->queuedFrames()>1)return 3;
            totalDropped+=players[i]->droppedFrames();
        }
        if(totalDropped<4)return 4;
        spin8(500);
        widget->grab().save(app.arguments().at(2)+"/stage8-four.png");
        widget->switch_to_channel1();spin8(200);
        widget->switch_to_channel4();spin8(500);
        int stopped=frames[0],other=frames[1];players[0]->stop();spin8(600);
        if(frames[0]!=stopped || frames[1]<=other)return 5;
        widget->stopAll();int old=frames[1];spin8(300);
        if(frames[1]!=old)return 6;
        QJsonObject metrics;
        for(int i=0;i<4;i++) metrics[QString("displayedFrames%1").arg(i+1)]=frames[i];
        metrics["droppedDuringGuiStall"]=totalDropped;
        metrics["mailboxCapacityPerChannel"]=1;
        QFile output(app.arguments().at(2)+"/stage8.json");
        if(!output.open(QIODevice::WriteOnly))return 7;
        output.write(QJsonDocument(metrics).toJson());output.close();
        // 再次打开四路，立即销毁整个窗口，验证线程和进程被回收。
        for(int i=0;i<4;i++)widget->openSource(i,app.arguments().at(1));
        spin8(500);
        return 0;
    }();
    delete widget;
    return result;
}
