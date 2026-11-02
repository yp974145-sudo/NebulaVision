#include <QApplication>
#include <QElapsedTimer>
#include <QThread>
#include <QPushButton>
#include <QDebug>
#include <functional>
#include "view/mainwidget.h"
#include "view/monitorwidget.h"
void spinFix(int ms){QElapsedTimer t;t.start();while(t.elapsed()<ms){QApplication::processEvents();QThread::msleep(10);}}
bool waitFix(const std::function<bool()> &ready){QElapsedTimer t;t.start();while(t.elapsed()<6000){spinFix(20);if(ready())return true;}return false;}
int main(int argc,char **argv){
 QApplication app(argc,argv);MainWidget window;window.setAttribute(Qt::WA_DontShowOnScreen);window.show();window.resize(1000,700);spinFix(100);
 qInfo()<<"window"<<window.size()<<"minimum hint"<<window.minimumSizeHint();
 if(window.width()>1000 || window.height()>700)return 1;
 MonitorWidget *monitor=window.findChild<MonitorWidget *>();monitor->switch_to_channel4();
 int frames[4]={};FfmpegPlayer *players[4];
 for(int i=0;i<4;i++){players[i]=monitor->findChild<FfmpegPlayer *>(QString("previewPlayer%1").arg(i));QObject::connect(players[i],&FfmpegPlayer::frameDisplayed,[&frames,i](){++frames[i];});monitor->openSource(i,app.arguments().at(1));}
 if(!waitFix([&](){return frames[0]>3&&frames[1]>3&&frames[2]>3&&frames[3]>3;}))return 2;
 window.grab().save(app.arguments().at(3)+"/window1000x700.png");
 QPushButton *pause=monitor->findChild<QPushButton *>("pauseAll"),*resume=monitor->findChild<QPushButton *>("resumeAll"),*stop=monitor->findChild<QPushButton *>("stopAll");
 pause->click();spinFix(100);int before[4];for(int i=0;i<4;i++)before[i]=frames[i];spinFix(500);for(int i=0;i<4;i++)if(frames[i]!=before[i])return 3;
 resume->click();if(!waitFix([&](){return frames[0]>before[0]&&frames[1]>before[1]&&frames[2]>before[2]&&frames[3]>before[3];}))return 4;
 stop->click();for(int i=0;i<4;i++)before[i]=frames[i];resume->click();if(!waitFix([&](){return frames[0]>before[0]&&frames[1]>before[1]&&frames[2]>before[2]&&frames[3]>before[3];}))return 5;
 monitor->stopAll();monitor->openSource(0,app.arguments().at(2));if(!waitFix([&](){DecodeTask *task=players[0]->findChild<DecodeTask *>();return task && !task->isRunning();}))return 8;int ended=frames[0];qInfo()<<"EOF frames"<<ended;players[0]->resume();if(!waitFix([&](){return frames[0]>ended;}))return 6;
 monitor->stopAll();window.resize(1250,800);spinFix(100);window.resize(1000,700);spinFix(100);if(window.width()>1000||window.height()>700)return 7;
 return 0;
}
