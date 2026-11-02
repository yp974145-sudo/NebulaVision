#include <QApplication>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QElapsedTimer>
#include <QThread>
#include "util/winmanager.h"
#include "service/apiclient.h"

// 用本机HTTP替身控制延迟，复现“点击返回后登录请求才完成”。
static void waitFor(int milliseconds)
{
    QElapsedTimer timer;
    timer.start();
    while(timer.elapsed()<milliseconds) {
        QApplication::processEvents();
        QThread::msleep(5);
    }
}
int main(int argc,char **argv)
{
    QApplication app(argc,argv);
    app.setQuitOnLastWindowClosed(false);
    QTcpServer server;
    if(!server.listen(QHostAddress::LocalHost,0))return 1;
    qputenv("NEBULAVISION_API_URL",QString("http://127.0.0.1:%1").arg(server.serverPort()).toUtf8());
    bool reject=true;
    int loginRequests=0;
    QObject::connect(&server,&QTcpServer::newConnection,&app,[&]() {
        QTcpSocket *socket=server.nextPendingConnection();
        QObject::connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
        QObject::connect(socket,&QTcpSocket::readyRead,socket,[&,socket]() {
            QByteArray buffer=socket->property("buffer").toByteArray()+socket->readAll();
            socket->setProperty("buffer",buffer);
            if(socket->property("handled").toBool())return;
            int headerEnd=buffer.indexOf("\r\n\r\n");
            if(headerEnd<0)return;
            int length=0;
            for(const QByteArray &line:buffer.left(headerEnd).split('\n')) {
                if(line.toLower().startsWith("content-length:"))length=line.mid(15).trimmed().toInt();
            }
            if(buffer.size()<headerEnd+4+length)return;
            socket->setProperty("handled",true);
            bool login=buffer.startsWith("POST /api/login ");
            if(login)++loginRequests;
            bool denied=login && reject;
            QByteArray body=login ? (denied ? "{}" : "{\"token\":\"delayed-test-token\"}") : "{\"cameras\":[]}";
            QByteArray response="HTTP/1.1 "+QByteArray(denied?"401 Unauthorized":"200 OK")+"\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: "+QByteArray::number(body.size())+"\r\n\r\n"+body;
            QTimer::singleShot(login && !reject ? 500 : 0,socket,[socket,response]() {
                socket->write(response);socket->disconnectFromHost();
            });
        });
    });
    QString error;
    ApiClient::request("/api/login",{{"username","x"},{"password","x"}},true,&error);
    if(!error.contains("用户名或密码错误"))return 2;
    ApiClient::request("/api/me",{},false,&error);
    WinManager *manager=WinManager::getInstence();
    manager->mainWidget.show();
    manager->mainWidget.to_login_win();
    QPushButton *back=manager->loginWidget.findChild<QPushButton *>("login_back");
    if(!back)return 3;
    back->click();waitFor(50);
    if(manager->loginWidget.isVisible() || !manager->mainWidget.isVisible())return 4;
    manager->mainWidget.to_login_win();
    manager->loginWidget.close();waitFor(50);
    if(manager->loginWidget.isVisible() || !manager->mainWidget.isVisible())return 5;
    reject=false;
    manager->mainWidget.to_login_win();
    auto prepare=[&]() {
        manager->loginWidget.findChild<QLineEdit *>("login_username")->setText("test-user");
        manager->loginWidget.findChild<QLineEdit *>("login_password")->setText("test-password");
        auto captcha=manager->loginWidget.findChild<VerifyCodeLabel *>();
        manager->loginWidget.findChild<QLineEdit *>("edit_vcode")->setText(captcha->code());
    };
    prepare();manager->loginWidget.login();
    waitFor(100);back->click();waitFor(800);
    if(!ApiSession::token().isEmpty() || manager->loginWidget.isVisible() || !manager->mainWidget.isVisible())return 6;
    manager->mainWidget.to_login_win();prepare();manager->loginWidget.login();waitFor(1000);
    if(ApiSession::token()!="delayed-test-token" || manager->loginWidget.isVisible() || !manager->mainWidget.isVisible())return 7;
    if(loginRequests!=3)return 8;
    ApiSession::setToken({});
    WinManager::releaseInstence();
    qInfo("PASS: login error, back button, window close, late reply ignored, reopen and login");
    return 0;
}
