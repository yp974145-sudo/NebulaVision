// 在Xvfb中运行真实Qt控件。使用公开文件样本，不冒充实体USB摄像头验收。
#include <QApplication>
#include <QElapsedTimer>
#include <QThread>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QVideoSink>
#include <QVideoFrame>
#include <QVBoxLayout>
#include <cstdio>
#include <QFontDatabase>
#include <QCryptographicHash>
#include <QUuid>
#include "util/winmanager.h"
#include "util/logredaction.h"
#include "service/apiclient.h"
static bool until(const std::function<bool()> &condition,int timeout=45000){
    QElapsedTimer timer;timer.start();
    while(timer.elapsed()<timeout){QApplication::processEvents();if(condition())return true;QThread::msleep(10);}
    return false;
}
int runClipRecovery(QApplication &,int,char **);
int main(int argc,char **argv){
    QApplication app(argc,argv);app.setQuitOnLastWindowClosed(false);installLogRedaction();
    if(argc!=4)return 1;
    bool chineseFont=QFontDatabase::families().contains("Noto Sans CJK SC");
    printf("LINUX_CJK_FONT=%s\n",chineseFont?"PASS":"FAIL");if(!chineseFont)return 6;
    QString resources=QString::fromLocal8Bit(argv[1]),camera=QString::fromLocal8Bit(argv[2]),clip=QString::fromLocal8Bit(argv[3]);
    QFile file(resources+"/accounts.local.json");if(!file.open(QIODevice::ReadOnly))return 2;
    QJsonObject accounts=QJsonDocument::fromJson(file.readAll()).object();QString error;
    auto login=ApiClient::request("/api/login",accounts.value("admin").toObject(),true,&error);
    if(login.value("token").toString().isEmpty())return 3;ApiSession::setToken(login.value("token").toString());
    auto manager=WinManager::getInstence();manager->mainWidget.resize(1200,850);manager->mainWidget.showUsername("ci_admin");
    auto monitor=manager->mainWidget.findChild<MonitorWidget *>();int frames=0;
    auto player=monitor->findChild<FfmpegPlayer *>("previewPlayer0");QObject::connect(player,&FfmpegPlayer::frameDisplayed,&app,[&](){++frames;});
    monitor->openSource(0,clip);bool monitorOkay=until([&](){return frames>=5;});monitor->stopAll();
    printf("LINUX_MONITOR_FRAMES=%s\n",monitorOkay?"PASS":"FAIL");
    QWidget host;QVBoxLayout layout(&host);auto preview=new LivePreview(&host);layout.addWidget(preview);host.resize(900,650);host.show();preview->setCameraId(camera);preview->openStream(QUrl::fromLocalFile(clip).toString());
    bool gesture=until([&](){return !preview->lastSavedEvent.isEmpty();});
    bool saved=gesture && until([&](){return preview->findChild<QLabel *>("clip_status")->text().contains("异常录像已保存");});
    if(!saved)printf("MODEL_STATUS=%s\nCLIP_STATUS=%s\n",preview->findChild<QLabel *>("live_status")->text().toUtf8().constData(),preview->findChild<QLabel *>("clip_status")->text().toUtf8().constData());
    QString event=preview->lastSavedEvent;preview->stop();host.hide();printf("LINUX_REAL_GESTURE_AND_CLIP=%s\n",saved?"PASS":"FAIL");
    manager->mainWidget.switch_to_review();auto review=manager->mainWidget.findChild<ReviewWidget *>();auto table=review->findChild<QTableWidget *>("recordTable");int row=-1;
    bool linked=saved && until([&](){for(int i=0;i<table->rowCount();++i)if(table->item(i,0) && table->item(i,0)->data(Qt::UserRole+1).toString()==event && table->item(i,0)->data(Qt::UserRole).toString().toLongLong()>0){row=i;return true;}return false;});
    auto media=review->findChild<QMediaPlayer *>("reviewPlayer");auto sink=review->findChild<QVideoSink *>("reviewVideoSink");int replayFrames=0;
    QObject::connect(sink,&QVideoSink::videoFrameChanged,&app,[&](const QVideoFrame &frame){if(frame.isValid())++replayFrames;});
    if(linked)table->cellDoubleClicked(row,0);
    bool replay=linked && until([&](){return replayFrames>=2 && media->duration()>1000 && media->isSeekable();});
    if(replay){media->pause();until([&](){return media->playbackState()==QMediaPlayer::PausedState;});int target=int(media->duration()/2);review->findChild<QSlider *>("reviewSeek")->setValue(target);replay=until([&](){return qAbs(media->position()-target)<500;});review->findChild<QComboBox *>("reviewSpeed")->setCurrentIndex(3);replay=replay && qFuzzyCompare(media->playbackRate(),2.);media->play();replay=replay && until([&](){return media->position()>target+150;});}
    replay=replay && review->isVisible() && !monitor->isVisible();
    QString output=QString::fromUtf8(qgetenv("NEBULAVISION_TEST_OUTPUT"));if(!output.isEmpty())manager->mainWidget.grab().save(output+"/linux-playback.png");
    printf("LINUX_IN_PAGE_REPLAY_SEEK_2X=%s\n",replay?"PASS":"FAIL");
    review->clearPlayback();QStringList retryIds;
    for(int i=0;i<2;i++){
        QString name="Linux retry fixture "+QUuid::createUuid().toString(QUuid::WithoutBraces);
        auto device=ApiClient::request("/api/cameras/register",{{"name",name},{"sourceType","local"},{"sourceKey",QString::fromLatin1(QCryptographicHash::hash(name.toUtf8(),QCryptographicHash::Sha256).toHex())}},true,&error);retryIds.append(device.value("id").toString());
    }
    ApiClient::request("/api/logout",{},true,&error);
    QByteArray xArg=resources.toLocal8Bit(),first=retryIds[0].toLocal8Bit(),second=retryIds[1].toLocal8Bit();char *args[]={const_cast<char *>("clip-recovery"),xArg.data(),first.data(),second.data()};
    int recovered=runClipRecovery(app,4,args);printf("LINUX_RETRY_RECOVERY_EMPTY_SELECTION_SEQUENCE=%s\n",recovered==0?"PASS":"FAIL");
    ApiSession::setToken({});WinManager::releaseInstence();
    return monitorOkay && saved && replay && recovered==0?0:5;
}
