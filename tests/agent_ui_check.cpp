#include <QApplication>
#include <QDialog>
#include <QPlainTextEdit>
#include <QJsonDocument>
#include <QElapsedTimer>
#include <QThread>
#include "util/winmanager.h"
#include "service/apiclient.h"
static bool waitText(QPlainTextEdit *output,const QString &needle) {
    QElapsedTimer timer;timer.start();
    while(timer.elapsed()<20000){QApplication::processEvents();if(output->toPlainText().contains(needle))return true;QThread::msleep(10);}
    return false;
}
int main(int argc,char **argv) {
    QApplication app(argc,argv);
    if(argc!=2)return 1;
    QFile file(QString::fromLocal8Bit(argv[1])+"/本机登录信息.local.json");
    if(!file.open(QIODevice::ReadOnly))return 2;
    QJsonObject account=QJsonDocument::fromJson(file.readAll()).object().value("admin").toObject();file.close();
    QString error;QJsonObject login=ApiClient::request("/api/login",account,true,&error);
    QString token=login.value("token").toString();if(token.isEmpty())return 3;
    ApiSession::setToken(token);
    auto manager=WinManager::getInstence();manager->mainWidget.showUsername("test-admin");
    manager->mainWidget.findChild<QPushButton *>("query_assistant")->click();
    auto dialog=manager->mainWidget.findChild<QDialog *>("query_assistant_dialog");
    if(!dialog)return 4;
    auto input=dialog->findChild<QLineEdit *>("assistant_question");
    auto run=dialog->findChild<QPushButton *>("assistant_run");
    auto output=dialog->findChild<QPlainTextEdit *>("assistant_output");
    run->click();bool business=waitText(output,"当前账号可见设备数");
    if(!business)fprintf(stdout,"Business result: %s\n",output->toPlainText().toUtf8().constData());
    input->setText("文档 登录后返回按钮没有反应怎么办");run->click();
    bool retrieval=waitText(output,"登录与返回修复说明.md");
    if(!retrieval)fprintf(stdout,"Retrieval result: %s\n",output->toPlainText().toUtf8().constData());
    ApiClient::request("/api/logout",{},true,&error);ApiSession::setToken({});
    dialog->close();WinManager::releaseInstence();
    if(!business || !retrieval)return 5;
    fprintf(stdout,"PASS: original Qt window to Python, authenticated API report, real vector retrieval\n");
    return 0;
}
