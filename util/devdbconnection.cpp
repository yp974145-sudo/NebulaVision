#include "devdbconnection.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSqlQuery>
#include <QVariant>
#include <QUuid>

DevDbConnection::DevDbConnection()
{
    // 给每个对象生成独立的连接名称，避免覆盖原项目的默认连接。
    QString uniqueId = QUuid::createUuid().toString();
    connectionName = "nebulavision_dev_" + uniqueId;
}

DevDbConnection::~DevDbConnection()
{
    // 对象销毁时，自动清理它管理的连接。
    close();
}

QSqlDatabase DevDbConnection::database() const
{
    return db;
}

void DevDbConnection::close()
{
    // 尚未创建有效连接时，无需清理。
    if (db.isValid() == false) {
        return;
    }

    db.close();                                  // 关闭连接。
    db = QSqlDatabase();                         // 释放本对象持有的句柄。
    QSqlDatabase::removeDatabase(connectionName); // 从Qt中移除连接。
    // 调用者必须先释放使用这条连接的查询对象和其他句柄。
}

bool DevDbConnection::open(const QString &configPath, QString *error)
{
    // 1. 清理上次连接和旧错误，允许同一对象重新尝试连接。
    close();
    if (error != nullptr) {
        error->clear();
    }

    // Lambda：定义一个只在open()内部使用的小函数，统一处理失败。
    // [this, error]：保存当前对象指针和错误字符串指针，供函数体使用。
    // 不保存这个Lambda，也不交给其他线程；只在本次open()返回前调用。
    auto fail = [this, error](const QString &message) -> bool {
        close();
        if (error != nullptr) {
            *error = message;
        }
        return false;
    };

    // 2. 以只读方式打开本地配置文件。
    QFile file(configPath);
    bool fileOpened = file.open(QIODevice::ReadOnly);
    if (fileOpened == false) {
        return fail("Cannot read local database config");
    }

    // 3. 读取全部文字，按照JSON格式解析。
    QByteArray text = file.readAll();
    file.close();
    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(text, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        return fail("Invalid database JSON config");
    }
    if (document.isObject() == false) {
        return fail("Invalid database JSON config");
    }

    // 4. 按字段名称取出配置。真实密码只保存在被Git忽略的本地文件里。
    QJsonObject config = document.object();
    QString databaseName = config.value("database").toString();
    QString username = config.value("username").toString();
    QString password = config.value("password").toString();

    // 5. 当前阶段只允许连接新库，并使用指定的只读账号。
    if (databaseName != "nebulavision_dev") {
        return fail("Rejected database: expected nebulavision_dev");
    }
    if (username != "nebulavision_viewer_v1") {
        return fail("Rejected account: expected dedicated read-only viewer");
    }
    if (password.isEmpty() == true) {
        return fail("Missing database password in local config");
    }

    // 6. 注册一条命名连接，填写参数。这时还没有连接服务器。
    db = QSqlDatabase::addDatabase("QMYSQL", connectionName);
    db.setHostName("127.0.0.1");
    db.setPort(3306);
    db.setDatabaseName(databaseName);
    db.setUserName(username);
    db.setPassword(password);
    // 连接服务器的超时选项，不是所有SQL查询的超时设置。
    db.setConnectOptions("MYSQL_OPT_CONNECT_TIMEOUT=3;MYSQL_OPT_READ_TIMEOUT=5;MYSQL_OPT_WRITE_TIMEOUT=5");

    // 7. 真正连接MySQL。失败时不把密码写进错误信息。
    bool connected = db.open();
    if (connected == false) {
        return fail("Database connection failed: check server, driver and local credentials");
    }

    // 8. 向服务器核对实际库名和授权账号。
    // 单独调用函数：返回后，里面的query已经销毁，可以安全清理连接。
    bool correctConnection = checkConnection();
    if (correctConnection == false) {
        return fail("Database or account verification failed");
    }

    return true;
}

bool DevDbConnection::checkConnection()
{
    QSqlQuery query(db);

    // 返回两列：当前数据库、MySQL实际匹配的授权账号。
    bool success = query.exec("SELECT DATABASE(), CURRENT_USER()");
    if (success == false) {
        return false;
    }

    // next()移动到第一行；没有结果就不能继续读取。
    bool hasRow = query.next();
    if (hasRow == false) {
        return false;
    }

    // 列号从0开始，顺序与上面SELECT中的顺序一致。
    QString actualDatabase = query.value(0).toString();
    QString actualAccount = query.value(1).toString();
    if (actualDatabase != "nebulavision_dev") {
        return false;
    }
    if (actualAccount != "nebulavision_viewer_v1@127.0.0.1") {
        return false;
    }

    return true;
}
