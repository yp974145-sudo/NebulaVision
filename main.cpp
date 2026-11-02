#include <QApplication>
#include <QDir>
#include "util/winmanager.h"
#include "util/dbconn.h"
#include "util/devdbconnection.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QStringList arguments = a.arguments();

    // 带--check-dev-db启动时，只检查新库连接，不打开原窗口或访问旧库。
    if (arguments.contains("--check-dev-db") == true) {
        QString executableDirectory = QCoreApplication::applicationDirPath();
        QDir directory(executableDirectory);
        QString path = directory.filePath("../NebulaVision/config/viewer.local.json");

        // 测试时可用--dev-db-config 配置路径，指定另一份配置文件。
        int option = arguments.indexOf("--dev-db-config");
        if (option >= 0) {
            if (option + 1 < arguments.size()) {
                path = arguments.at(option + 1);
            }
        }

        DevDbConnection connection;
        QString error;
        bool success = connection.open(path, &error);
        if (success == false) {
            qWarning().noquote() << error;
            return 2; // 非0退出码表示检查失败。
        }
        qInfo() << "Connected to nebulavision_dev with dedicated read-only account";
        return 0; // 检查成功；connection离开作用域时自动释放连接。
    }

    // 普通启动仍使用原窗口。阶段2再把新库查询接入页面。
    WinManager::getInstence()->mainWidget.show();
    int result = a.exec();

    // 在QApplication销毁之前释放窗口和原数据库连接。
    WinManager::releaseInstence();
    DbConn::releaseInstence();
    return result;
}
