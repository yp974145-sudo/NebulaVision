#include "apiclient.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QEventLoop>
#include <QTimer>
#include <QJsonDocument>
#include <QMutex>
#include <QMutexLocker>
#include <QUrl>
static QMutex sessionMutex;
static QString sessionToken;
QString ApiSession::token(){QMutexLocker lock(&sessionMutex);return sessionToken;}
void ApiSession::setToken(const QString &token){QMutexLocker lock(&sessionMutex);sessionToken=token;}
QJsonObject ApiClient::request(const QString &path,const QJsonObject &body,bool post,QString *error,const QString &tokenOverride) {
    if(error)error->clear();
    QString base=QString::fromUtf8(qgetenv("NEBULAVISION_API_URL"));
    if(base.isEmpty())base="http://127.0.0.1:18880";
    QUrl url(base+path);
    // 本机HTTP开发模式；远程地址必须使用HTTPS，避免明文发送凭据。
    if(!url.isValid() || (url.scheme()!="https" && !(url.scheme()=="http" && url.host()=="127.0.0.1"))) {
        if(error)*error="API地址无效，远程服务需要HTTPS";return {};
    }
    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");
    QString token=tokenOverride.isNull()?ApiSession::token():tokenOverride;
    if(!token.isEmpty())request.setRawHeader("Authorization",("Bearer "+token).toUtf8());
    QNetworkReply *reply=post?manager.post(request,QJsonDocument(body).toJson(QJsonDocument::Compact)):manager.get(request);
    QEventLoop loop;QTimer timeout;timeout.setSingleShot(true);
    QObject::connect(&timeout,&QTimer::timeout,reply,&QNetworkReply::abort);
    QObject::connect(reply,&QNetworkReply::finished,&loop,&QEventLoop::quit);
    timeout.start(path=="/api/events/clip"?25000:8000);loop.exec();
    int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QJsonDocument document=QJsonDocument::fromJson(reply->readAll());
    if(reply->error()!=QNetworkReply::NoError || status<200 || status>=300 || !document.isObject()) {
        if(error) {
            if(status==401)*error=path=="/api/login" ? "用户名或密码错误，或账号已停用。请使用新系统账号。" : "请先登录，或登录已过期";
            else if(status==429)*error="操作过于频繁，请稍后再试";
            else if(status==403)*error="当前账号没有操作权限";
            else if(status==409 && path=="/api/users")*error="用户名已存在，请换一个用户名";
            else if(status==400 && path=="/api/users")*error="账号格式错误：用户名须为3至32位字母、数字或下划线，密码至少6位";
            else if(status>0)*error=QString("API请求失败（HTTP %1）").arg(status);
            else *error="服务端未连接或请求超时，请检查本机服务";
        }
        return {};
    }
    return document.object();
}

#include <QSaveFile>
bool ApiClient::download(const QString &path,const QString &target,QString *error){
    if(error)error->clear();
    QString base=QString::fromUtf8(qgetenv("NEBULAVISION_API_URL"));if(base.isEmpty())base="http://127.0.0.1:18880";
    QUrl url(base+path);
    if(!url.isValid() || (url.scheme()!="https" && !(url.scheme()=="http" && url.host()=="127.0.0.1"))){if(error)*error="下载地址无效";return false;}
    QSaveFile file(target);if(!file.open(QIODevice::WriteOnly)){if(error)*error="本地缓存不可写";return false;}
    QNetworkAccessManager manager;QNetworkRequest request(url);request.setRawHeader("Authorization",("Bearer "+ApiSession::token()).toUtf8());
    QNetworkReply *reply=manager.get(request);QEventLoop loop;QTimer timeout;timeout.setSingleShot(true);qint64 bytes=0;bool failed=false;QByteArray preview;
    QObject::connect(reply,&QNetworkReply::readyRead,[&](){QByteArray chunk=reply->readAll();if(preview.size()<4096)preview.append(chunk.left(4096-preview.size()));bytes+=chunk.size();if(bytes>64*1024*1024 || file.write(chunk)!=chunk.size()){failed=true;reply->abort();}});
    QObject::connect(reply,&QNetworkReply::finished,&loop,&QEventLoop::quit);QObject::connect(&timeout,&QTimer::timeout,reply,&QNetworkReply::abort);timeout.start(15000);loop.exec();
    int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if(failed || reply->error()!=QNetworkReply::NoError || status!=200 || bytes==0){file.cancelWriting();if(error)*error=QString("录像下载失败（HTTP %1），检查登录权限或文件状态").arg(status)+" "+QJsonDocument::fromJson(preview).object().value("error").toString();return false;}
    if(!file.commit()){if(error)*error="保存录像缓存失败";return false;}return true;
}
