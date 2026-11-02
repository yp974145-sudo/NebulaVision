#ifndef APISERVER_H
#define APISERVER_H
#include <QObject>
#include <QHttpServer>
#include <QTcpServer>
#include <QJsonObject>
#include <QUrlQuery>
#include <QSqlDatabase>
#include <QMutex>
#include <QDateTime>

// 请求快照可以安全交给工作线程；不跨线程持有原始HTTP请求对象。
struct ApiRequest {
    QString path;
    QHttpServerRequest::Method method;
    QJsonObject body;
    QUrlQuery query;
    QString token;
};
class ApiServer : public QObject {
public:
    explicit ApiServer(const QJsonObject &config);
    bool listen();
private:
    QHttpServerResponse handle(const ApiRequest &request);
    QMutex sessionMutex;
    QHash<QString,QPair<qint64,QDateTime>> sessions;
    QHash<QString,QPair<int,qint64>> loginAttempts;
    QHash<qint64,QPair<int,qint64>> fallbackLimits;
    QJsonObject config;
    QHttpServer http;
    QTcpServer tcp;
};
#endif
