#include <QApplication>
#include "util/winmanager.h"
#include "util/dbconn.h"


int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    WinManager::getInstence()->mainWidget.show();
    int result = a.exec();
    // 在 QApplication 销毁前释放窗口。
    WinManager::releaseInstence();
    DbConn::releaseInstence();
    return result;
}
