#include <QVariant>
#include "devdbconnection.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSqlQuery>
#include <QUuid>
DevDbConnection::DevDbConnection()
    : connectionName("nebulavision_dev_" + QUuid::createUuid().toString()) {}
DevDbConnection::~DevDbConnection() { close(); }
void DevDbConnection::close() {
    if (!db.isValid()) return;
    db.close();
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName);
}
bool DevDbConnection::open(const QString &configPath, QString *error) {
    close();
    if (error) error->clear();
    auto fail = [&](const QString &message) { close(); if (error) *error = message; return false; };
    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly)) return fail("Cannot read local database config");
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return fail("Invalid database JSON config");
    const auto config = document.object();
    if (config.value("database").toString() != "nebulavision_dev")
        return fail("Rejected database: expected nebulavision_dev");
    if (config.value("username").toString() != "nebulavision_viewer_v1")
        return fail("Rejected account: expected dedicated read-only viewer");
    const QString password = config.value("password").toString();
    if (password.isEmpty()) return fail("Missing database password in local config");
    db = QSqlDatabase::addDatabase("QMYSQL", connectionName);
    db.setHostName("127.0.0.1");
    db.setPort(3306);
    db.setDatabaseName("nebulavision_dev");
    db.setUserName("nebulavision_viewer_v1");
    db.setPassword(password);
    db.setConnectOptions("MYSQL_OPT_CONNECT_TIMEOUT=3");
    if (!db.open()) return fail("Database connection failed: check server, driver and local credentials");
    bool verified = false;
    {
        QSqlQuery query(db);
        verified = query.exec("SELECT DATABASE(), CURRENT_USER()") && query.next()
            && query.value(0).toString() == "nebulavision_dev"
            && query.value(1).toString() == "nebulavision_viewer_v1@127.0.0.1";
    }
    if (!verified) return fail("Database or account verification failed");
    return true;
}

