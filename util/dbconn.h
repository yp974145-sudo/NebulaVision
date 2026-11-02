#ifndef DBCONN_H
#define DBCONN_H
#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlDatabase>
#include <QCryptographicHash>
class DbConn
{
public:
    static DbConn *getInstence();
    static void releaseInstence();
    DbConn (const DbConn &)=delete ;
    DbConn& operator=(const DbConn&)=delete ;
    QSqlDatabase getDb() const;
    QString getMd5(const QString &str);
private:
    DbConn();
    ~DbConn();
    static DbConn * s_instence;
    QSqlDatabase db;
};

#endif // DBCONN_H
