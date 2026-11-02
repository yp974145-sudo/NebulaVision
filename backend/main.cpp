#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QThreadPool>
#include "apiserver.h"
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    QString path=QString::fromUtf8(qgetenv("NEBULAVISION_SERVER_CONFIG"));
    if(path.isEmpty()) path=QCoreApplication::applicationDirPath()+"/../NebulaVision/config/server.local.json";
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly)) {qCritical("Missing server local config");return 2;}
    QJsonDocument doc=QJsonDocument::fromJson(file.readAll());
    if(!doc.isObject())return 2;
    ApiServer server(doc.object());
    if(!server.listen())return 3;
    int result=app.exec();
    QThreadPool::globalInstance()->waitForDone();
    return result;
}
