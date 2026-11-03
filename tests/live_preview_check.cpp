#include <QApplication>
#include <QMediaDevices>
#include <QCameraDevice>
#include <QElapsedTimer>
#include <QThread>
#include <QJsonDocument>
#include <QJsonArray>
#include <QPlainTextEdit>
#include <cstdio>
#include "view/livepreview.h"
#include "util/winmanager.h"
#include "service/apiclient.h"
#include "util/logredaction.h"
#include <QUrl>
#include <QDir>
static bool waitFrames(LivePreview *preview,int frames){QElapsedTimer timer;timer.start();while(timer.elapsed()<25000){QApplication::processEvents();if(preview->processedFrames>=frames)return true;QThread::msleep(10);}fprintf(stdout,"Preview status: %s\n",preview->findChild<QLabel *>("live_status")->text().toUtf8().constData());return false;}
int main(int argc,char **argv){
    QApplication app(argc,argv);app.setQuitOnLastWindowClosed(false);if(argc!=2)return 1;
    installLogRedaction();
    QDir tools(QString::fromLocal8Bit(argv[1])+"/media-tools/imageio-ffmpeg/imageio_ffmpeg/binaries");auto binaries=tools.entryList({"ffmpeg*.exe"},QDir::Files);if(binaries.isEmpty())return 9;qputenv("NEBULAVISION_FFMPEG",tools.absoluteFilePath(binaries.first()).toUtf8());
    QFile file(QString::fromLocal8Bit(argv[1])+"/本机登录信息.local.json");if(!file.open(QIODevice::ReadOnly))return 2;
    auto account=QJsonDocument::fromJson(file.readAll()).object().value("admin").toObject();file.close();QString error;
    auto login=ApiClient::request("/api/login",account,true,&error);if(login.value("token").toString().isEmpty())return 3;ApiSession::setToken(login.value("token").toString());
    auto devices=ApiClient::request("/api/cameras",{},false,&error).value("cameras").toArray();if(devices.isEmpty())return 4;
    QString id=devices.first().toObject().value("id").toString();auto stream=ApiClient::request("/api/media/play",{{"cameraId",id}},true,&error);
    auto preview=new LivePreview;preview->show();preview->openStream(stream.value("url").toString());
    bool network=waitFrames(preview,5) && preview->lastPeople>=1;
    QString screenshot=QString::fromUtf8(qgetenv("NEBULAVISION_TEST_OUTPUT"));
    if(network && !screenshot.isEmpty())preview->grab().save(screenshot+"/live-preview-demo-test.png");
    // 默认不保留截图；只有调用者设置测试输出路径时保存演示画面。
    preview->stop();QApplication::processEvents();
    network=network && preview->findChild<QLabel *>("live_status")->text().contains("已停止");
    preview->close();QApplication::processEvents();
    fprintf(stdout,"RTSP continuous frames and people: %s\n",network?"PASS":"FAIL");
    auto local=QMediaDevices::videoInputs();bool cameraPass=!local.isEmpty();
    if(cameraPass)for(int cycle=0;cycle<2;cycle++){
        auto camera=new LivePreview;camera->show();camera->openCamera(local.first());bool passed=waitFrames(camera,3);cameraPass=cameraPass&&passed;
        camera->stop();camera->close();QApplication::processEvents();QThread::msleep(300);
    }
    fprintf(stdout,"USB continuous detection and reopen: %s\n",cameraPass?"PASS":"FAIL");
    bool gesturePass=true;QString clip=QString::fromUtf8(qgetenv("NEBULAVISION_TEST_GESTURE_CLIP"));
    if(!clip.isEmpty()){
        auto gesture=new LivePreview;gesture->show();gesture->openStream(QUrl::fromLocalFile(clip).toString());
        gesturePass=waitFrames(gesture,5) && gesture->gestureAlert;
        if(gesturePass && !screenshot.isEmpty())gesture->grab().save(screenshot+"/gesture-alert-test.png");
        bool cleared=false;QElapsedTimer clearTimer;clearTimer.start();
        while(gesturePass && clearTimer.elapsed()<20000){QApplication::processEvents();if(!gesture->gestureAlert && gesture->processedFrames>=8){cleared=true;break;}QThread::msleep(10);}
        gesturePass=gesturePass&&cleared;
        gesture->stop();gesture->close();QApplication::processEvents();
        fprintf(stdout,"Real hand video -> gesture alert: %s\n",gesturePass?"PASS":"FAIL");
    }
    auto manager=WinManager::getInstence();manager->mainWidget.show();manager->mainWidget.findChild<QPushButton *>("backend_panel")->click();
    auto panel=manager->mainWidget.findChild<QDialog *>("backend_dialog");bool panelPass=false;QElapsedTimer timer;timer.start();
    while(panel && timer.elapsed()<10000){QApplication::processEvents();auto label=panel->findChild<QLabel *>("backend_state");if(label && label->text().contains("后台已运行")){panelPass=true;break;}QThread::msleep(10);}
    ApiClient::request("/api/logout",{},true,&error);ApiSession::setToken({});
    if(panelPass){
        panel->findChild<QPushButton *>("backend_stop")->click();timer.restart();bool stopped=false;
        while(timer.elapsed()<15000){QApplication::processEvents();if(panel->findChild<QLabel *>("backend_state")->text().contains("后台未全部就绪") && panel->findChild<QPushButton *>("backend_start")->isEnabled()){stopped=true;break;}QThread::msleep(10);}
        panel->findChild<QPushButton *>("backend_start")->click();timer.restart();bool started=false;
        while(timer.elapsed()<30000){QApplication::processEvents();if(panel->findChild<QLabel *>("backend_state")->text().contains("后台已运行") && panel->findChild<QPushButton *>("backend_stop")->isEnabled()){started=true;break;}QThread::msleep(10);}
        panelPass=stopped&&started;
    }
    if(panel)panel->close();fprintf(stdout,"Backend status window: %s\n",panelPass?"PASS":"FAIL");
    WinManager::releaseInstence();
    return network && cameraPass && panelPass && gesturePass?0:5;
}
