#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QThreadPool>
#include "apiserver.h"
#include "serverdb.h"
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    QString path=QString::fromUtf8(qgetenv("NEBULAVISION_SERVER_CONFIG"));
    if(path.isEmpty()) path=QCoreApplication::applicationDirPath()+"/../NebulaVision/config/server.local.json";
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly)) {qCritical("Missing server local config");return 2;}
    QJsonDocument doc=QJsonDocument::fromJson(file.readAll());
    file.close(); // 配置读完即关闭，不长期锁住本地配置文件。
    if(!doc.isObject())return 2;
    ApiServer server(doc.object());
    if(!server.listen())return 3;
    {
        // 已独占监听端口后才恢复旧任务，防止第二个实例干扰正在录制的实例。
        ServerDb db(doc.object());QSqlQuery recovery(db.db);
        recovery.exec("UPDATE recordings SET status='interrupted' WHERE status='writing'");
        recovery.exec("UPDATE detection_jobs SET status='failed',error_code='SERVER_RESTARTED' WHERE status IN ('queued','running')");
    }
    int result=app.exec();
    QThreadPool::globalInstance()->waitForDone();
    return result;
}
