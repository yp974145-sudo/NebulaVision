#include <QApplication>
#include "util/winmanager.h"
#include "util/dbconn.h"
#include "util/devdbconnection.h"
#include <QDir>


int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    // Explicit diagnostic mode: no legacy windows or old database access.
    if (a.arguments().contains("--check-dev-db")) {
        QString path = QDir(QCoreApplication::applicationDirPath()).filePath("../NebulaVision/config/viewer.local.json");
        const int option = a.arguments().indexOf("--dev-db-config");
        if (option >= 0 && option + 1 < a.arguments().size()) path = a.arguments().at(option + 1);
        DevDbConnection connection;
        QString error;
        if (!connection.open(path, &error)) { qWarning().noquote() << error; return 2; }
        qInfo() << "Connected to nebulavision_dev with dedicated read-only account";
        return 0;
    }
    WinManager::getInstence()->mainWidget.show();
    int result = a.exec();
    // 在 QApplication 销毁前释放窗口。
    WinManager::releaseInstence();
    DbConn::releaseInstence();
    return result;
}
