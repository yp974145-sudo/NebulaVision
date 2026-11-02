#include "apiserver.h"
#include <QtConcurrent>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QUuid>
#include <QHostAddress>
#include <QFileInfo>

using Status=QHttpServerResponder::StatusCode;
static QHttpServerResponse reply(const QJsonObject &data,int code=200) {
    return QHttpServerResponse(data,static_cast<Status>(code));
}
static QHttpServerResponse fail(const QString &message,int code) {
    return reply(QJsonObject{{"error",message}},code);
}
// 每个工作请求拥有自己的连接；query必须在这个对象之前销毁。
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
ApiServer::ApiServer(const QJsonObject &config):config(config) {
    http.route("/health",[](){return reply(QJsonObject{{"status","ok"},{"service","NebulaVision C++ API"}});});
    QStringList paths={"/api/events","/api/cameras"};
    for(const QString &path:paths) {
        http.route(path,[this](const QHttpServerRequest &request){
            ApiRequest snapshot;
            snapshot.path=request.url().path();snapshot.method=request.method();
            snapshot.query=request.query();snapshot.body=QJsonDocument::fromJson(request.body()).object();
            snapshot.token=QString::fromUtf8(request.value("Authorization"));
            // 工作线程执行数据库操作，HTTP事件循环不等待同步SQL。
            return QtConcurrent::run([this,snapshot](){return handle(snapshot);});
        });
    }
}
bool ApiServer::listen() {
    int port=config.value("port").toInt(18880);
    if(port<1024 || port>65535)return false;
    if(!tcp.listen(QHostAddress::LocalHost,quint16(port)))return false;
    return http.bind(&tcp);
}
QHttpServerResponse ApiServer::handle(const ApiRequest &request) {
    if(request.method!=QHttpServerRequest::Method::Get)return fail("method not allowed",405);
    ServerDb connection(config);
    if(!connection.db.isOpen())return fail("database unavailable",503);
    QSqlQuery query(connection.db);
    if(request.path=="/api/cameras") {
        if(!query.exec("SELECT id,name,status,enabled FROM cameras ORDER BY id"))return fail("camera query failed",500);
        QJsonArray rows;
        while(query.next())rows.append(QJsonObject{{"id",query.value(0).toString()},{"name",query.value(1).toString()},
            {"status",query.value(2).toString()},{"enabled",query.value(3).toBool()}});
        return reply(QJsonObject{{"cameras",rows}});
    }
    bool ok=true;
    int page=request.query.queryItemValue("page").toInt(&ok);
    if(request.query.queryItemValue("page").isEmpty()){page=1;ok=true;}
    if(!ok || page<1 || page>1000000)return fail("invalid page",400);
    int size=10;
    if(request.query.hasQueryItem("size")){size=request.query.queryItemValue("size").toInt(&ok);}
    if(!ok || size<1 || size>100)return fail("invalid size",400);
    QString day=request.query.queryItemValue("date");
    QDate date=QDate::fromString(day,Qt::ISODate);
    if(!day.isEmpty() && (!date.isValid() || date.toString(Qt::ISODate)!=day))return fail("invalid date",400);
    QString where;
    if(!day.isEmpty())where=" WHERE e.occurred_at>=:begin AND e.occurred_at<:end";
    auto bind=[date,day](QSqlQuery &q){if(!day.isEmpty()){
        q.bindValue(":begin",date.toString(Qt::ISODate)+" 00:00:00");
        q.bindValue(":end",date.addDays(1).toString(Qt::ISODate)+" 00:00:00");}};
    query.prepare("SELECT COUNT(*) FROM events e"+where);bind(query);
    if(!query.exec() || !query.next())return fail("event count failed",500);
    int total=query.value(0).toInt();int pages=qMax(1,(total+size-1)/size);page=qMin(page,pages);
    query.finish();
    query.prepare("SELECT e.id,c.name,e.occurred_at,e.event_type,e.description,e.status FROM events e LEFT JOIN cameras c ON c.id=e.camera_id"+where+" ORDER BY e.occurred_at DESC,e.id DESC LIMIT :size OFFSET :offset");
    bind(query);query.bindValue(":size",size);query.bindValue(":offset",(page-1)*size);
    if(!query.exec())return fail("event query failed",500);
    QJsonArray rows;
    while(query.next())rows.append(QJsonObject{{"id",query.value(0).toString()},{"cameraName",query.value(1).toString()},
        {"occurredAt",query.value(2).toDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz")},{"type",query.value(3).toString()},
        {"description",query.value(4).toString()},{"status",query.value(5).toString()}});
    return reply(QJsonObject{{"events",rows},{"total",total},{"page",page},{"pages",pages}});
}
