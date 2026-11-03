#include "apiserver.h"
#include "mediaadapter.h"
#include "serverdb.h"
#include "recorder.h"
#include "redisclient.h"
#include "detectionjobs.h"
#include <QRegularExpression>
#include <QDir>
#include <QFile>
#include <QtConcurrent>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QUuid>
#include <QHostAddress>
#include <QFileInfo>
#include <QPasswordDigestor>
#include <QRandomGenerator>
#include <QMutexLocker>

using Status=QHttpServerResponder::StatusCode;
static QHttpServerResponse reply(const QJsonObject &data,int code=200) {
    return QHttpServerResponse(data,static_cast<Status>(code));
}
static QHttpServerResponse fail(const QString &message,int code) {
    return reply(QJsonObject{{"error",message}},code);
}
// 每个工作请求拥有自己的连接；query必须在这个对象之前销毁。
ApiServer::ApiServer(const QJsonObject &config):config(config) {
    http.route("/health",[](){return reply(QJsonObject{{"status","ok"},{"service","NebulaVision C++ API"}});});
    QStringList paths={"/api/events","/api/cameras","/api/camera","/api/login","/api/logout","/api/me","/api/media/play","/api/media/publish","/internal/media/auth","/api/recordings","/api/recordings/capture","/api/recordings/file","/api/status","/api/cameras/heartbeat","/api/detections/start","/api/detections/job"};
    paths.append("/api/users");
    paths.append("/api/cameras/register");paths.append("/api/cameras/source");paths.append("/api/events/gesture");
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
    ServerDb connection(config);
    if(!connection.db.isOpen())return fail("database unavailable",503);
    QSqlQuery auth(connection.db);
    if(request.path=="/internal/media/auth") {
        if(request.method!=QHttpServerRequest::Method::Post)return fail("method",405);
        QString token=request.body.value("token").toString();
        if(token.isEmpty()) token=QUrlQuery(request.body.value("query").toString()).queryItemValue("token");
        QJsonObject claims=MediaAdapter::verify(token,config.value("media_secret").toString());
        if(claims.isEmpty())return fail("media authentication required",401);
        if(claims.value("action")!=request.body.value("action") || claims.value("path")!=request.body.value("path"))return fail("media scope forbidden",403);
        qint64 owner=0;
        {
            QMutexLocker lock(&sessionMutex);
            auto session=sessions.value(claims.value("session").toString());
            if(session.second<QDateTime::currentDateTimeUtc())return fail("media session expired",401);
            owner=session.first;
        }
        auth.prepare("SELECT status FROM users WHERE id=?");auth.addBindValue(owner);
        if(!auth.exec() || !auth.next() || auth.value(0).toString()!="active")return fail("media account disabled",403);
        auth.prepare("SELECT 1 FROM user_roles ur JOIN roles r ON r.id=ur.role_id WHERE ur.user_id=? AND r.code='admin'");auth.addBindValue(owner);
        bool ownerAdmin=auth.exec() && auth.next();
        if(claims.value("action").toString()=="publish" && !ownerAdmin)return fail("publish forbidden",403);
        if(!ownerAdmin) {
            auth.prepare("SELECT 1 FROM user_camera_permissions WHERE user_id=? AND camera_id=?");auth.addBindValue(owner);auth.addBindValue(claims.value("cameraId").toString());
            if(!auth.exec() || !auth.next())return fail("media camera forbidden",403);
        }
        return reply(QJsonObject{{"expireAt",QDateTime::fromSecsSinceEpoch(qint64(claims.value("expires").toDouble()),Qt::UTC).toString(Qt::ISODate)}});
    }

    if(request.path=="/api/login") {
        if(request.method!=QHttpServerRequest::Method::Post)return fail("method not allowed",405);
        QString username=request.body.value("username").toString();
        QString password=request.body.value("password").toString();
        if(username.isEmpty() || username.size()>64 || password.size()>256)return fail("invalid login",400);
        {
            QMutexLocker lock(&sessionMutex);
            qint64 now=QDateTime::currentSecsSinceEpoch();
            if(loginAttempts.size()>2048)loginAttempts.clear();
            auto &attempt=loginAttempts[username];
            if(now-attempt.second>60){attempt.first=0;attempt.second=now;}
            if(++attempt.first>10)return fail("login rate exceeded",429);
        }
        auth.prepare("SELECT id,password_hash,status FROM users WHERE username=?");auth.addBindValue(username);
        if(!auth.exec())return fail("login unavailable",503);
        bool found=auth.next();
        qint64 userId=found?auth.value(0).toLongLong():0;
        QString stored=found?auth.value(1).toString():QString();
        bool active=found && auth.value(2).toString()=="active";
        QStringList parts=stored.split('$');
        QByteArray salt=parts.size()==4?QByteArray::fromBase64(parts[2].toLatin1()):QByteArray(16,'x');
        QByteArray expected=parts.size()==4?QByteArray::fromBase64(parts[3].toLatin1()):QByteArray(32,'x');
        QByteArray derived=QPasswordDigestor::deriveKeyPbkdf2(QCryptographicHash::Sha256,password.toUtf8(),salt,200000,32);
        unsigned difference=unsigned(derived.size()^expected.size());
        for(int i=0;i<qMin(derived.size(),expected.size());i++)difference|=unsigned(uchar(derived[i])^uchar(expected[i]));
        if(!active || parts.size()!=4 || parts[0]!="pbkdf2_sha256" || parts[1]!="200000" || difference!=0)return fail("invalid credentials",401);
        QByteArray random(32,0);for(int i=0;i<random.size();i++)random[i]=char(QRandomGenerator::system()->generate()&255);
        QString token=QString::fromLatin1(random.toBase64(QByteArray::Base64UrlEncoding|QByteArray::OmitTrailingEquals));
        QString key=QString::fromLatin1(QCryptographicHash::hash(token.toUtf8(),QCryptographicHash::Sha256).toHex());
        int ttl=qBound(2,config.value("token_ttl_seconds").toInt(3600),86400);
        {
            QMutexLocker lock(&sessionMutex);
            for(auto it=sessions.begin();it!=sessions.end();) {
                if(it.value().second<QDateTime::currentDateTimeUtc())it=sessions.erase(it);else ++it;
            }
            if(sessions.size()>=1024)return fail("session capacity reached",503);
            sessions[key]={userId,QDateTime::currentDateTimeUtc().addSecs(ttl)};
        }
        auth.prepare("UPDATE users SET last_login_at=UTC_TIMESTAMP() WHERE id=?");auth.addBindValue(userId);auth.exec();
        return reply(QJsonObject{{"token",token},{"username",username},{"expiresIn",ttl}});
    }
    if(!request.token.startsWith("Bearer "))return fail("authentication required",401);
    QString key=QString::fromLatin1(QCryptographicHash::hash(request.token.mid(7).toUtf8(),QCryptographicHash::Sha256).toHex());
    qint64 userId=0;
    {
        QMutexLocker lock(&sessionMutex);
        if(!sessions.contains(key))return fail("invalid session",401);
        auto session=sessions.value(key);
        if(session.second<QDateTime::currentDateTimeUtc()){sessions.remove(key);return fail("expired session",401);}
        userId=session.first;
        if(request.path=="/api/logout") {
            if(request.method!=QHttpServerRequest::Method::Post)return fail("method not allowed",405);
            sessions.remove(key);return reply(QJsonObject{{"loggedOut",true}});
        }
    }
    auth.prepare("SELECT status FROM users WHERE id=?");auth.addBindValue(userId);
    if(!auth.exec() || !auth.next() || auth.value(0).toString()!="active")return fail("account disabled",401);
    auth.prepare("SELECT 1 FROM user_roles ur JOIN roles r ON r.id=ur.role_id WHERE ur.user_id=? AND r.code='admin'");auth.addBindValue(userId);
    bool admin=auth.exec() && auth.next();

    int redisPort=config.value("redis_port").toInt(16379);
    int limit=config.value("rate_per_minute").toInt(120);
    bool redisOk=false;
    QString rateKey=QString("nv:rate:%1:%2").arg(userId).arg(QDateTime::currentSecsSinceEpoch()/60);
    QVariant rate=RedisClient::command(redisPort,QStringList()<<"EVAL"<<"local n=redis.call('INCR',KEYS[1]); if n==1 then redis.call('EXPIRE',KEYS[1],ARGV[1]); end; return n"<<"1"<<rateKey<<"60",&redisOk);
    if(redisOk){if(rate.toLongLong()>limit)return fail("rate limit exceeded",429);}
    else {
        // Redis故障时仍有进程内保护，但不宣称跨进程共享限流。
        QMutexLocker lock(&sessionMutex);auto &counter=fallbackLimits[userId];qint64 minute=QDateTime::currentSecsSinceEpoch()/60;
        if(counter.second!=minute){counter={0,minute};}if(++counter.first>limit)return fail("local rate limit exceeded",429);
    }
    if(request.path=="/api/cameras/register") {
        if(!admin)return fail("admin required",403);
        if(request.method!=QHttpServerRequest::Method::Post)return fail("method",405);
        QString type=request.body.value("sourceType").toString(),name=request.body.value("name").toString().trimmed();
        QString source=request.body.value("url").toString(),sourceKey=request.body.value("sourceKey").toString();
        if(name.isEmpty() || name.size()>128 || (type!="local" && type!="rtsp"))return fail("invalid device",400);
        if(type=="local") {if(!QRegularExpression("^[a-f0-9]{64}$").match(sourceKey).hasMatch())return fail("invalid source key",400);source.clear();}
        else {QUrl url(source);if(source.size()>1024 || !url.isValid() || url.host().isEmpty() || (url.scheme()!="rtsp" && url.scheme()!="rtsps"))return fail("invalid RTSP URL",400);sourceKey=QString::fromLatin1(QCryptographicHash::hash(source.toUtf8(),QCryptographicHash::Sha256).toHex());}
        auth.prepare("INSERT INTO cameras(camera_key,name,stream_url,enabled,source_type,source_key) VALUES (?,?,?,1,?,?) ON DUPLICATE KEY UPDATE id=LAST_INSERT_ID(id)");
        auth.addBindValue("cam-"+QUuid::createUuid().toString(QUuid::WithoutBraces));auth.addBindValue(name);auth.addBindValue(source);auth.addBindValue(type);auth.addBindValue(sourceKey);
        if(!auth.exec())return fail("device insert failed",500);
        return reply(QJsonObject{{"id",auth.lastInsertId().toString()},{"sourceType",type}},201);
    }
    if(request.path=="/api/cameras/source") {
        bool valid=false;qint64 id=request.query.queryItemValue("id").toLongLong(&valid);if(!valid || id<=0)return fail("invalid camera",400);
        auth.prepare("SELECT stream_url,source_type,enabled FROM cameras WHERE id=?");auth.addBindValue(id);
        if(!auth.exec() || !auth.next())return fail("camera missing",404);
        QString url=auth.value(0).toString(),type=auth.value(1).toString();bool enabled=auth.value(2).toBool();auth.finish();
        if(!admin){auth.prepare("SELECT 1 FROM user_camera_permissions WHERE user_id=? AND camera_id=?");auth.addBindValue(userId);auth.addBindValue(id);if(!auth.exec() || !auth.next())return fail("camera forbidden",403);}
        if(!enabled)return fail("camera disabled",409);
        if(type!="rtsp")return fail("not a direct RTSP device",400);
        return reply(QJsonObject{{"url",url},{"sourceType",type}});
    }
    if(request.path=="/api/events/gesture") {
        if(request.method!=QHttpServerRequest::Method::Post)return fail("method",405);
        bool valid=false;qint64 camera=request.body.value("cameraId").toString().toLongLong(&valid);
        QString eventKey=request.body.value("eventKey").toString();double confidence=request.body.value("confidence").toDouble(-1);
        if(!valid || camera<=0 || !QRegularExpression("^[a-f0-9-]{36}$").match(eventKey).hasMatch() || !qIsFinite(confidence) || confidence<.6 || confidence>1 || request.body.value("gesture").toString()!="Open_Palm" || request.body.value("consecutive").toInt()<3)return fail("invalid gesture event",400);
        if(!admin){auth.prepare("SELECT 1 FROM user_camera_permissions WHERE user_id=? AND camera_id=?");auth.addBindValue(userId);auth.addBindValue(camera);if(!auth.exec() || !auth.next())return fail("camera forbidden",403);}
        // 事件、审计和回执一起成功或一起撤销，避免界面看到半条入库结果。
        auth.finish();if(!connection.db.transaction())return fail("transaction unavailable",500);
        auto abort=[&](int code){auth.finish();connection.db.rollback();return fail("event save failed",code);};
        // 按摄像头加行锁，让同一设备的并发上报依次检查10秒合并规则。
        auth.prepare("SELECT enabled FROM cameras WHERE id=? FOR UPDATE");auth.addBindValue(camera);if(!auth.exec() || !auth.next())return abort(404);if(!auth.value(0).toBool())return abort(409);auth.finish();
        auth.prepare("SELECT event_id,camera_id FROM live_event_receipts WHERE request_key=?");auth.addBindValue(eventKey);
        if(!auth.exec())return abort(500);
        if(auth.next()){QString id=auth.value(0).toString();if(auth.value(1).toLongLong()!=camera)return abort(409);auth.finish();if(!connection.db.commit())return abort(500);return reply(QJsonObject{{"id",id},{"reused",true}});}
        auto receipt=[&](const QString &id){auth.finish();auth.prepare("INSERT INTO live_event_receipts(request_key,event_id,camera_id,actor_id) VALUES (?,?,?,?)");auth.addBindValue(eventKey);auth.addBindValue(id);auth.addBindValue(camera);auth.addBindValue(userId);return auth.exec();};
        auth.finish();auth.prepare("SELECT id FROM events WHERE camera_id=? AND event_type='gesture_open_palm' AND occurred_at>UTC_TIMESTAMP(3)-INTERVAL 10 SECOND ORDER BY id DESC LIMIT 1");auth.addBindValue(camera);
        if(!auth.exec())return abort(500);
        if(auth.next()){QString id=auth.value(0).toString();if(!receipt(id))return abort(500);auth.finish();if(!connection.db.commit())return abort(500);return reply(QJsonObject{{"id",id},{"reused",true}});}
        auth.finish();auth.prepare("INSERT INTO events(event_key,camera_id,occurred_at,event_type,description,confidence,model_version,status) VALUES (?,?,UTC_TIMESTAMP(3),'gesture_open_palm',?,?,?,'new')");
        auth.addBindValue("live:"+eventKey);auth.addBindValue(camera);auth.addBindValue("客户端实时模型上报：张开手掌连续3次，置信度达到0.60；无自动录像。");auth.addBindValue(confidence);auth.addBindValue("mediapipe-gesture-v1");
        if(!auth.exec())return abort(500);QString id=auth.lastInsertId().toString();auth.finish();
        auth.prepare("INSERT INTO audit_logs(actor_id,action,resource_type,resource_id) VALUES (?,'report_gesture','event',?)");auth.addBindValue(userId);auth.addBindValue(id);if(!auth.exec())return abort(500);auth.finish();
        if(!receipt(id))return abort(500);auth.finish();if(!connection.db.commit())return abort(500);qInfo().noquote()<<"gesture_event_saved camera="<<camera<<" event="<<id;return reply(QJsonObject{{"id",id},{"reused",false}},201);
    }
    if(request.path=="/api/users") {
        if(!admin)return fail("admin required",403);
        if(request.method!=QHttpServerRequest::Method::Post)return fail("method",405);
        QString name=request.body.value("username").toString().trimmed();
        QString password=request.body.value("password").toString();
        QJsonArray cameras=request.body.value("cameraIds").toArray();
        if(!QRegularExpression("^[A-Za-z0-9_]{3,32}$").match(name).hasMatch() || password.size()<6 || password.size()>128 || cameras.size()>100)
            return fail("invalid account",400);
        if(request.body.contains("role") && request.body.value("role").toString()!="viewer")return fail("viewer role only",400);
        for(const QJsonValue &camera:cameras){
            bool ok=false;qint64 id=camera.toString().toLongLong(&ok);if(!ok || id<=0)return fail("invalid camera",400);
            auth.prepare("SELECT 1 FROM cameras WHERE id=?");auth.addBindValue(id);
            if(!auth.exec())return fail("database error",500);
            if(!auth.next())return fail("camera missing",404);
        }
        auth.finish();
        QByteArray salt(16,0);for(int i=0;i<salt.size();i++)salt[i]=char(QRandomGenerator::system()->generate()&255);
        QByteArray hash=QPasswordDigestor::deriveKeyPbkdf2(QCryptographicHash::Sha256,password.toUtf8(),salt,200000,32);
        QString stored="pbkdf2_sha256$200000$"+QString::fromLatin1(salt.toBase64())+"$"+QString::fromLatin1(hash.toBase64());
        if(!connection.db.transaction())return fail("transaction unavailable",500);
        auto abort=[&](int code){auth.finish();connection.db.rollback();return fail("account creation failed",code);};
        auth.prepare("INSERT INTO users(username,password_hash,status) VALUES (?,?,'active')");auth.addBindValue(name);auth.addBindValue(stored);
        if(!auth.exec())return abort(auth.lastError().nativeErrorCode()=="1062"?409:500);
        QString id=auth.lastInsertId().toString();
        auth.prepare("INSERT INTO user_roles(user_id,role_id) SELECT ?,id FROM roles WHERE code='viewer'");auth.addBindValue(id);
        if(!auth.exec() || auth.numRowsAffected()!=1)return abort(500);
        for(const QJsonValue &camera:cameras){
            auth.prepare("INSERT IGNORE INTO user_camera_permissions(user_id,camera_id) VALUES (?,?)");auth.addBindValue(id);auth.addBindValue(camera.toString());
            if(!auth.exec())return abort(500);
        }
        auth.finish();if(!connection.db.commit())return abort(500);
        return reply(QJsonObject{{"id",id},{"username",name},{"role","viewer"}},201);
    }
    if(request.path=="/api/status")return reply(QJsonObject{{"redis",redisOk?"available":"unavailable"},{"rateMode",redisOk?"redis":"local-fallback"}});
    if(request.path=="/api/cameras/heartbeat") {
        if(request.method!=QHttpServerRequest::Method::Post)return fail("method",405);
        if(!admin)return fail("heartbeat admin required",403);
        bool valid=false;qint64 camera=request.body.value("cameraId").toString().toLongLong(&valid);
        int ttl=request.body.value("ttl").toInt(6);if(!valid || camera<=0 || ttl<2 || ttl>60)return fail("invalid heartbeat",400);
        auth.prepare("SELECT 1 FROM cameras WHERE id=?");auth.addBindValue(camera);if(!auth.exec()||!auth.next())return fail("camera missing",404);
        bool ok=false;RedisClient::command(redisPort,QStringList()<<"SET"<<QString("nv:camera:%1").arg(camera)<<"online"<<"EX"<<QString::number(ttl),&ok);
        if(!ok)return fail("heartbeat storage unavailable",503);
        return reply(QJsonObject{{"status","online"},{"ttl",ttl}});
    }
    if(request.path=="/api/me")return reply(QJsonObject{{"id",QString::number(userId)},{"admin",admin}});

    if(request.path=="/api/media/play" || request.path=="/api/media/publish") {
        if(request.method!=QHttpServerRequest::Method::Post)return fail("method",405);
        bool publishing=request.path.endsWith("publish");
        if(publishing && !admin)return fail("publish forbidden",403);
        bool valid=false;qint64 camera=request.body.value("cameraId").toString().toLongLong(&valid);
        if(!valid || camera<=0)return fail("invalid camera",400);
        auth.prepare("SELECT camera_key,enabled FROM cameras WHERE id=?");auth.addBindValue(camera);
        if(!auth.exec() || !auth.next())return fail("camera missing",404);
        QString path=auth.value(0).toString();
        if(!auth.value(1).toBool())return fail("camera disabled",409);
        if(!admin) {
            auth.prepare("SELECT 1 FROM user_camera_permissions WHERE user_id=? AND camera_id=?");auth.addBindValue(userId);auth.addBindValue(camera);
            if(!auth.exec() || !auth.next())return fail("camera forbidden",403);
        }
        QDateTime expires=QDateTime::currentDateTimeUtc().addSecs(3600);
        {QMutexLocker lock(&sessionMutex);expires=qMin(expires,sessions.value(key).second);}
        QJsonObject claims{{"session",key},{"cameraId",QString::number(camera)},{"path",path},
            {"action",publishing?"publish":"read"},{"expires",double(expires.toSecsSinceEpoch())}};
        QString token=MediaAdapter::sign(claims,config.value("media_secret").toString());
        QString url="rtsp://127.0.0.1:18554/"+path+"?token="+token;
        return reply(QJsonObject{{"url",url},{"expiresAt",expires.toString(Qt::ISODate)}});
    }

    if(request.path.startsWith("/api/recordings")) {
        if(request.path=="/api/recordings/capture") {
            if(request.method!=QHttpServerRequest::Method::Post)return fail("method",405);
            if(!admin)return fail("recording admin required",403);
            bool valid=false;qint64 camera=request.body.value("cameraId").toString().toLongLong(&valid);
            int seconds=request.body.value("seconds").toInt(4);
            if(!valid || camera<=0 || seconds<2 || seconds>30)return fail("invalid capture",400);
            auth.prepare("SELECT camera_key,enabled,source_type,stream_url FROM cameras WHERE id=?");auth.addBindValue(camera);
            if(!auth.exec() || !auth.next())return fail("camera missing",404);
            if(!auth.value(1).toBool())return fail("camera disabled",409);
            if(auth.value(2).toString()=="local")return fail("local recording requires client capture",409);
            QString stream=auth.value(0).toString();
            QDateTime expires=QDateTime::currentDateTimeUtc().addSecs(120);
            QJsonObject claims{{"session",key},{"cameraId",QString::number(camera)},{"path",stream},{"action","read"},{"expires",double(expires.toSecsSinceEpoch())}};
            QString url="rtsp://127.0.0.1:18554/"+stream+"?token="+MediaAdapter::sign(claims,config.value("media_secret").toString());
            if(auth.value(2).toString()=="rtsp")url=auth.value(3).toString();
            QString filename=QUuid::createUuid().toString(QUuid::WithoutBraces)+".mp4";
            auth.prepare("INSERT INTO recordings(camera_id,started_at,object_key,status) VALUES (?,UTC_TIMESTAMP(3),?,'writing')");auth.addBindValue(camera);auth.addBindValue(filename);
            if(!auth.exec())return fail("recording insert failed",500);
            QString id=auth.lastInsertId().toString();
            Recorder::start(config,id,filename,url,seconds);
            return reply(QJsonObject{{"id",id},{"status","writing"}},202);
        }
        if(request.method!=QHttpServerRequest::Method::Get)return fail("method",405);
        QString permission=admin?QString():QString(" AND EXISTS(SELECT 1 FROM user_camera_permissions p WHERE p.user_id=%1 AND p.camera_id=r.camera_id)").arg(userId);
        if(request.path=="/api/recordings/file") {
            bool valid=false;qint64 id=request.query.queryItemValue("id").toLongLong(&valid);
            if(!valid || id<=0)return fail("invalid recording",400);
            auth.prepare("SELECT object_key,status FROM recordings r WHERE id=?"+permission);auth.addBindValue(id);
            if(!auth.exec() || !auth.next())return fail("recording missing or forbidden",404);
            if(auth.value(1).toString()!="ready")return fail("recording not ready",409);
            QString filename=auth.value(0).toString();
            if(QFileInfo(filename).fileName()!=filename)return fail("invalid storage key",500);
            QString path=QDir(config.value("recording_root").toString()).filePath(filename);
            QFileInfo file(path);
            if(!file.isFile())return fail("recording file missing",410);
            if(file.size()>64*1024*1024)return fail("recording exceeds development download limit",413);
            return QHttpServerResponse::fromFile(path);
        }
        QString day=request.query.queryItemValue("date");QDate date=QDate::fromString(day,Qt::ISODate);
        if(!day.isEmpty() && (!date.isValid() || date.toString(Qt::ISODate)!=day))return fail("invalid date",400);
        bool valid=true;int page=request.query.queryItemValue("page").isEmpty()?1:request.query.queryItemValue("page").toInt(&valid);
        if(!valid || page<1 || page>1000000)return fail("invalid page",400);
        int size=request.query.queryItemValue("size").isEmpty()?10:request.query.queryItemValue("size").toInt(&valid);
        if(!valid || size<1 || size>100)return fail("invalid size",400);
        QString where=" WHERE 1=1"+permission;
        if(!day.isEmpty())where+=" AND r.started_at>=:begin AND r.started_at<:end";
        auto bind=[date,day](QSqlQuery &q){if(!day.isEmpty()){q.bindValue(":begin",date.toString(Qt::ISODate)+" 00:00:00");q.bindValue(":end",date.addDays(1).toString(Qt::ISODate)+" 00:00:00");}};
        auth.prepare("SELECT COUNT(*) FROM recordings r"+where);bind(auth);
        if(!auth.exec() || !auth.next())return fail("recording count failed",500);
        int total=auth.value(0).toInt();int pages=qMax(1,(total+size-1)/size);page=qMin(page,pages);auth.finish();
        auth.prepare("SELECT r.id,c.name,r.started_at,r.ended_at,r.bytes,r.status FROM recordings r JOIN cameras c ON c.id=r.camera_id"+where+" ORDER BY r.started_at DESC,r.id DESC LIMIT :size OFFSET :offset");bind(auth);auth.bindValue(":size",size);auth.bindValue(":offset",(page-1)*size);
        if(!auth.exec())return fail("recording list failed",500);
        QJsonArray rows;while(auth.next())rows.append(QJsonObject{{"id",auth.value(0).toString()},{"cameraName",auth.value(1).toString()},{"startedAt",auth.value(2).toDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz")},{"endedAt",auth.value(3).toDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz")},{"bytes",double(auth.value(4).toLongLong())},{"status",auth.value(5).toString()}});
        return reply(QJsonObject{{"recordings",rows},{"total",total},{"page",page},{"pages",pages}});
    }

    if(request.path=="/api/detections/start") {
        if(request.method!=QHttpServerRequest::Method::Post)return fail("method",405);
        if(!admin)return fail("detection admin required",403);
        QString jobKey=request.body.value("jobKey").toString();
        if(!QRegularExpression("^[A-Za-z0-9_-]{8,64}$").match(jobKey).hasMatch())return fail("invalid job key",400);
        bool valid=false;qint64 recording=request.body.value("recordingId").toString().toLongLong(&valid);
        if(request.body.contains("threshold") && !request.body.value("threshold").isDouble())return fail("threshold must be numeric",400);
        if(request.body.contains("roi") && !request.body.value("roi").isArray())return fail("region must be array",400);
        double threshold=request.body.value("threshold").toDouble(.25);QJsonArray roi=request.body.value("roi").toArray();
        for(const QJsonValue &coordinate:roi)if(!coordinate.isDouble() || !qIsFinite(coordinate.toDouble()))return fail("invalid region coordinate",400);
        if(!valid || recording<=0 || !qIsFinite(threshold) || threshold<.05 || threshold>.95)return fail("invalid detection parameters",400);
        if(!roi.isEmpty() && (roi.size()!=4 || !(0<=roi[0].toDouble() && roi[0].toDouble()<roi[2].toDouble() && roi[2].toDouble()<=1 && 0<=roi[1].toDouble() && roi[1].toDouble()<roi[3].toDouble() && roi[3].toDouble()<=1)))return fail("invalid region",400);
        auth.prepare("SELECT camera_id,object_key,status FROM recordings WHERE id=?");auth.addBindValue(recording);
        if(!auth.exec() || !auth.next())return fail("recording missing",404);
        QString camera=auth.value(0).toString();QString filename=auth.value(1).toString();if(auth.value(2).toString()!="ready")return fail("recording not ready",409);
        if(QFileInfo(filename).fileName()!=filename)return fail("invalid storage key",500);
        QString path=QDir(config.value("recording_root").toString()).filePath(filename);if(!QFileInfo(path).isFile())return fail("recording file missing",410);
        QByteArray input=QJsonDocument(QJsonObject{{"recordingId",QString::number(recording)},{"threshold",threshold},{"roi",roi},{"mode",config.value("detection_mode")},{"modelVersion",config.value("model_version")}}).toJson(QJsonDocument::Compact);
        QString fingerprint=QString::fromLatin1(QCryptographicHash::hash(input,QCryptographicHash::Sha256).toHex());
        auth.prepare("SELECT id,input_hash,owner_id,status FROM detection_jobs WHERE job_key=?");auth.addBindValue(jobKey);
        if(!auth.exec())return fail("job lookup failed",500);
        if(auth.next()){
            if(auth.value(1).toString()!=fingerprint || auth.value(2).toLongLong()!=userId)return fail("idempotency key reused with different payload",409);
            return reply(QJsonObject{{"id",auth.value(0).toString()},{"status",auth.value(3).toString()},{"reused",true}});
        }
        auth.exec("SELECT COUNT(*) FROM detection_jobs WHERE status IN ('queued','running')");if(auth.next() && auth.value(0).toInt()>=8)return fail("detection queue full",429);
        auth.prepare("INSERT INTO detection_jobs(job_key,owner_id,camera_id,recording_id,input_hash,status) VALUES (?,?,?,?,?,'queued')");auth.addBindValue(jobKey);auth.addBindValue(userId);auth.addBindValue(camera);auth.addBindValue(recording);auth.addBindValue(fingerprint);
        if(!auth.exec())return fail("job collision, retry same key",409);
        QString id=auth.lastInsertId().toString();DetectionJobs::start(config,id,QString::number(recording),path,float(threshold),roi);
        return reply(QJsonObject{{"id",id},{"status","queued"}},202);
    }
    if(request.path=="/api/detections/job") {
        bool valid=false;qint64 id=request.query.queryItemValue("id").toLongLong(&valid);if(!valid || id<=0)return fail("invalid job",400);
        auth.prepare("SELECT owner_id,status,result_json,error_code FROM detection_jobs WHERE id=?");auth.addBindValue(id);
        if(!auth.exec() || !auth.next())return fail("job missing",404);
        if(!admin && auth.value(0).toLongLong()!=userId)return fail("job forbidden",403);
        return reply(QJsonObject{{"id",QString::number(id)},{"status",auth.value(1).toString()},{"result",QJsonDocument::fromJson(auth.value(2).toByteArray()).object()},{"error",auth.value(3).toString()}});
    }
    if(request.method!=QHttpServerRequest::Method::Get)return fail("method not allowed",405);
    QSqlQuery query(connection.db);
    if(request.path=="/api/cameras") {
        if(!query.exec(QString("SELECT id,name,status,enabled,source_type,source_key FROM cameras c %1 ORDER BY id").arg(admin?QString():QString("WHERE EXISTS(SELECT 1 FROM user_camera_permissions p WHERE p.user_id=%1 AND p.camera_id=c.id)").arg(userId))))return fail("camera query failed",500);
        QJsonArray rows;
        while(query.next()) {
            bool ok=false;QString id=query.value(0).toString();
            QVariant alive=RedisClient::command(redisPort,QStringList()<<"GET"<<"nv:camera:"+id,&ok);
            QString status=ok?(alive.isValid()?"online":"offline"):"unknown";
            rows.append(QJsonObject{{"id",id},{"name",query.value(1).toString()},{"status",status},
                {"enabled",query.value(3).toBool()},{"sourceType",query.value(4).toString()},{"sourceKey",query.value(5).toString()},{"statusSource",ok?"redis":"unavailable"}});
        }
        return reply(QJsonObject{{"cameras",rows}});
    }
    if(request.path=="/api/camera") {
        bool valid=false;qint64 id=request.query.queryItemValue("id").toLongLong(&valid);
        if(!valid || id<=0)return fail("invalid camera",400);
        query.prepare("SELECT 1 FROM cameras WHERE id=?");query.addBindValue(id);
        if(!query.exec() || !query.next())return fail("camera not found",404);
        if(!admin) {
            query.prepare("SELECT 1 FROM user_camera_permissions WHERE user_id=? AND camera_id=?");query.addBindValue(userId);query.addBindValue(id);
            if(!query.exec() || !query.next())return fail("camera forbidden",403);
        }
        return reply(QJsonObject{{"id",QString::number(id)}});
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
    QString where=" WHERE 1=1";
    if(!admin)where+=QString(" AND EXISTS(SELECT 1 FROM user_camera_permissions p WHERE p.user_id=%1 AND p.camera_id=e.camera_id)").arg(userId);
    if(!day.isEmpty())where+=" AND e.occurred_at>=:begin AND e.occurred_at<:end";
    auto bind=[date,day](QSqlQuery &q){if(!day.isEmpty()){
        q.bindValue(":begin",date.toString(Qt::ISODate)+" 00:00:00");
        q.bindValue(":end",date.addDays(1).toString(Qt::ISODate)+" 00:00:00");}};
    query.prepare("SELECT COUNT(*) FROM events e"+where);bind(query);
    if(!query.exec() || !query.next())return fail("event count failed",500);
    int total=query.value(0).toInt();int pages=qMax(1,(total+size-1)/size);page=qMin(page,pages);
    query.finish();
    query.prepare("SELECT e.id,c.name,e.occurred_at,e.event_type,e.description,e.status,e.recording_id FROM events e LEFT JOIN cameras c ON c.id=e.camera_id"+where+" ORDER BY e.occurred_at DESC,e.id DESC LIMIT :size OFFSET :offset");
    bind(query);query.bindValue(":size",size);query.bindValue(":offset",(page-1)*size);
    if(!query.exec())return fail("event query failed",500);
    QJsonArray rows;
    while(query.next())rows.append(QJsonObject{{"id",query.value(0).toString()},{"cameraName",query.value(1).toString()},
        {"occurredAt",query.value(2).toDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz")},{"type",query.value(3).toString()},
        {"description",query.value(4).toString()},{"status",query.value(5).toString()},{"recordingId",query.value(6).isNull()?QString():query.value(6).toString()}});
    return reply(QJsonObject{{"events",rows},{"total",total},{"page",page},{"pages",pages}});
}
