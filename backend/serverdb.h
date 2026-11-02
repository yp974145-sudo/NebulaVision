#ifndef SERVERDB_H
#define SERVERDB_H
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QJsonObject>
#include <QUuid>
class ServerDb {
public:
    QSqlDatabase db;
    QString name;
    explicit ServerDb(const QJsonObject &config) {
        name="server_"+QUuid::createUuid().toString();
        db=QSqlDatabase::addDatabase("QMYSQL",name);
        if(config.value("database").toString()!="nebulavision_dev")return;
        db.setHostName("127.0.0.1");db.setPort(3306);
        db.setDatabaseName("nebulavision_dev");
        db.setUserName(config.value("username").toString());
        db.setPassword(config.value("password").toString());
        db.setConnectOptions("MYSQL_OPT_CONNECT_TIMEOUT=3;MYSQL_OPT_READ_TIMEOUT=5;MYSQL_OPT_WRITE_TIMEOUT=5");
        if(db.open()) {QSqlQuery q(db);q.exec("SET time_zone='+00:00'");}
    }
    ~ServerDb(){db.close();db=QSqlDatabase();QSqlDatabase::removeDatabase(name);}
};
#endif
