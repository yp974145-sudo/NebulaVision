#ifndef DEVDBCONNECTION_H
#define DEVDBCONNECTION_H

#include <QSqlDatabase>
#include <QString>

// 管理新库连接。对象的创建、使用和销毁应在同一个线程中进行。
class DevDbConnection
{
public:
    DevDbConnection();
    ~DevDbConnection();

    // configPath：配置文件路径。
    // error：接收错误原因的字符串地址；成功返回true，失败返回false。
    bool open(const QString &configPath, QString *error);

    // 返回连接句柄，供后续查询使用。句柄副本不是另一条新连接。
    QSqlDatabase database() const;

    // 禁止复制这个管理对象，避免两个对象重复清理同一条连接。
    DevDbConnection(const DevDbConnection &) = delete;
    DevDbConnection &operator=(const DevDbConnection &) = delete;

private:
    void close();
    bool checkConnection();

    QString connectionName; // Qt内部的连接名称，不是MySQL数据库名称。
    QSqlDatabase db;         // 用于操作数据库连接的句柄。
};
#endif
