// 隔离测试：旧会话失败、换账号拒绝、重建录制对象后恢复、无录像选择清空。
#include <QApplication>
#include <QElapsedTimer>
#include <QThread>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QUuid>
#include <QMediaPlayer>
#include "service/alertcliprecorder.h"
#include "service/apiclient.h"
#include "view/reviewwidget.h"
static bool until(const std::function<bool()> &condition){QElapsedTimer t;t.start();while(t.elapsed()<35000){QApplication::processEvents();if(condition())return true;QThread::msleep(10);}return false;}
int runClipRecovery(QApplication &app,int argc,char **argv){
    if(argc!=4)return 1;
    QString x=QString::fromLocal8Bit(argv[1]),camera=QString::fromLocal8Bit(argv[2]),emptyCamera=QString::fromLocal8Bit(argv[3]);
    QFile file(x+"/本机登录信息.local.json");if(!file.exists())file.setFileName(x+"/accounts.local.json");if(!file.open(QIODevice::ReadOnly))return 2;auto accounts=QJsonDocument::fromJson(file.readAll()).object();QString error;
    auto login=[&](const QString &role){auto data=ApiClient::request("/api/login",accounts.value(role).toObject(),true,&error);ApiSession::setToken(data.value("token").toString());return !ApiSession::token().isEmpty();};
    if(!login("admin"))return 3;
    QString key=QUuid::createUuid().toString(QUuid::WithoutBraces);auto body=QJsonObject{{"cameraId",camera},{"eventKey",key},{"gesture","Open_Palm"},{"confidence",.9},{"consecutive",3}};
    auto event=ApiClient::request("/api/events/gesture",body,true,&error).value("id").toString();if(event.isEmpty())return 4;
    QString testRoot=QString::fromUtf8(qgetenv("NEBULAVISION_TEST_OUTPUT"))+"/retry-resources";qputenv("NEBULAVISION_RESOURCES_ROOT",testRoot.toUtf8());
    auto recorder=new AlertClipRecorder;QString status;QObject::connect(recorder,&AlertClipRecorder::statusChanged,&app,[&](const QString &s){status=s;});recorder->restore(camera);
    QImage image(640,360,QImage::Format_RGB888);image.fill(Qt::darkBlue);for(int i=0;i<15;i++)recorder->sample(image);recorder->begin(key);
    ApiClient::request("/api/logout",{},true,&error);if(!login("admin"))return 5;
    for(int i=0;i<25;i++)recorder->sample(image);if(!until([&](){return status.contains("保存失败");}))return 6;
    QString path=testRoot+"/alert-cache/"+key+".mp4";if(!QFile::exists(path) || !QFile::exists(path+".pending.json"))return 7;
    delete recorder;recorder=new AlertClipRecorder;status.clear();QObject::connect(recorder,&AlertClipRecorder::statusChanged,&app,[&](const QString &s){status=s;});recorder->restore(camera);if(!status.contains("发现上次"))return 8;
    QString adminToken=ApiSession::token();if(!login("viewer"))return 9;QString viewerToken=ApiSession::token();recorder->retry();if(!until([&](){return status.contains("保存失败");}) || !QFile::exists(path))return 10;
    ApiSession::setToken(adminToken);recorder->retry();if(!until([&](){return status.contains("异常录像已保存");}))return 11;
    if(QFile::exists(path) || QFile::exists(path+".pending.json"))return 12;
    delete recorder;printf("PASS: expired-session failure, durable recovery, other account denied, same account fresh-session retry\n");
    body["cameraId"]=emptyCamera;body["eventKey"]=QUuid::createUuid().toString(QUuid::WithoutBraces);QString emptyEvent=ApiClient::request("/api/events/gesture",body,true,&error).value("id").toString();
    ReviewWidget review;review.resize(1100,800);review.show();review.refresh();auto table=review.findChild<QTableWidget *>("recordTable");int readyRow=-1,emptyRow=-1;
    if(!until([&](){for(int i=0;i<table->rowCount();i++){auto item=table->item(i,0);if(!item)continue;if(item->data(Qt::UserRole+1).toString()==event)readyRow=i;if(item->data(Qt::UserRole+1).toString()==emptyEvent)emptyRow=i;}return readyRow>=0 && emptyRow>=0;}))return 13;
    for(int i=0;i<table->rowCount();i++)if(table->item(i,0)->text()!=QString::number(i+1))return 14;
    table->cellDoubleClicked(readyRow,0);auto media=review.findChild<QMediaPlayer *>("reviewPlayer");if(!until([&](){return media->duration()>1000;}))return 15;
    table->setCurrentCell(emptyRow,0);QApplication::processEvents();if(!media->source().isEmpty() || media->playbackState()!=QMediaPlayer::StoppedState)return 16;
    table->cellDoubleClicked(emptyRow,0);if(!media->source().isEmpty())return 17;
    printf("PASS: continuous display sequence, stable database IDs, empty-event selection and double-click clear previous playback\n");
    review.clearPlayback();ApiClient::request("/api/logout",{},true,&error);ApiSession::setToken(viewerToken);ApiClient::request("/api/logout",{},true,&error);ApiSession::setToken({});return 0;
}
#ifndef NEBULAVISION_EMBED_RECOVERY
int main(int argc,char **argv){QApplication app(argc,argv);return runClipRecovery(app,argc,argv);}
#endif
