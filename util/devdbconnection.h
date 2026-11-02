#ifndef DEVDBCONNECTION_H
#define DEVDBCONNECTION_H
#include <QSqlDatabase>
#include <QString>
// Owns a named connection. Create/use/destroy on the same thread.
class DevDbConnection {
public:
    DevDbConnection();
    ~DevDbConnection();
    bool open(const QString &configPath, QString *error);
    QSqlDatabase database() const { return db; }
    DevDbConnection(const DevDbConnection &) = delete;
    DevDbConnection &operator=(const DevDbConnection &) = delete;
private:
    void close();
    QString connectionName;
    QSqlDatabase db;
};
#endif
