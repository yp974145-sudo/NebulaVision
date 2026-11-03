#include <QApplication>
#include <QJsonDocument>
#include <QJsonArray>
#include <QThread>
#include <QElapsedTimer>
#include <QDir>
#include "util/winmanager.h"
#include "util/logredaction.h"
#include "service/apiclient.h"
static bool until(const std::function<bool()> &condition,int milliseconds=25000){QElapsedTimer timer;timer.start();while(timer.elapsed()<milliseconds){QApplication::processEvents();if(condition())return true;QThread::msleep(10);}return false;}
int main(int argc,char **argv){
    QApplication app(argc,argv);app.setQuitOnLastWindowClosed(false);installLogRedaction();if(argc!=3)return 1;
    QString resources=QString::fromLocal8Bit(argv[1]),fixture=QString::fromLocal8Bit(argv[2]);QFile file(resources+"/本机登录信息.local.json");if(!file.open(QIODevice::ReadOnly))return 2;
    auto accounts=QJsonDocument::fromJson(file.readAll()).object();file.close();QString error;auto login=ApiClient::request("/api/login",accounts.value("admin").toObject(),true,&error);if(login.value("token").toString().isEmpty())return 3;ApiSession::setToken(login.value("token").toString());
    auto manager=WinManager::getInstence();manager->mainWidget.showUsername("admin");auto monitor=manager->mainWidget.findChild<MonitorWidget *>();auto list=monitor->findChild<QListWidget *>("cameraDevices");
    QJsonObject network,local;bool listed=until([&](){for(int i=0;i<list->count();i++){auto d=list->item(i)->data(Qt::UserRole).toJsonObject();if(d.value("id").toString()==fixture)network=d;if(d.value("sourceType").toString()=="local")local=d;}return !network.isEmpty() && !local.isEmpty();});if(!listed)return 4;
    monitor->openDevice(network);LivePreview *preview=nullptr;bool opened=until([&](){auto views=monitor->findChildren<LivePreview *>();if(!views.isEmpty())preview=views.first();return preview && preview->processedFrames>=5;});
    bool saved=opened && until([&](){return !preview->lastSavedEvent.isEmpty();});
    bool embedded=preview && !preview->isWindow() && preview->isVisible();
    if(saved){auto events=ApiClient::request("/api/events?size=100",{},false,&error).value("events").toArray();bool found=false;for(const auto &v:events)if(v.toObject().value("id").toString()==preview->lastSavedEvent)found=true;saved=found;}
    QString output=QString::fromUtf8(qgetenv("NEBULAVISION_TEST_OUTPUT"));if(!output.isEmpty())manager->mainWidget.grab().save(output+"/embedded-demo.png");
    printf("Embedded RTSP, actual model gesture -> database event visible: %s\n",saved&&embedded?"PASS":"FAIL");
    if(!saved && preview)printf("Event state: %s\n",preview->findChild<QLabel *>("event_save_status")->text().toUtf8().constData());
    if(!saved && preview)printf("Model state: %s\n",preview->findChild<QLabel *>("live_status")->text().toUtf8().constData());
    auto selector=monitor->findChild<QComboBox *>("channelSelector");selector->setCurrentIndex(1);monitor->openDevice(local);LivePreview *usb=nullptr;
    bool usbOkay=until([&](){for(auto view:monitor->findChildren<LivePreview *>())if(view!=preview && view->processedFrames>=3){usb=view;return true;}return false;});
    monitor->switch_to_channel4();QApplication::processEvents();bool four=usbOkay && usb->isVisible() && preview->isVisible() && !usb->isWindow();
    int before=usb?usb->processedFrames:0;if(usb)usb->pause();until([](){return false;},600);bool pauseOkay=usb && usb->processedFrames==before;if(usb)usb->resume();pauseOkay=pauseOkay && until([&](){return usb->processedFrames>before;});
    printf("Embedded USB, four-channel layout, pause/resume: %s\n",four&&pauseOkay?"PASS":"FAIL");
    monitor->stopAll();ApiClient::request("/api/logout",{},true,&error);ApiSession::setToken({});WinManager::releaseInstence();return saved&&embedded&&four&&pauseOkay?0:5;
}
