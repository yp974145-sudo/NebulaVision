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
QJsonObject ApiClient::request(const QString &path,const QJsonObject &body,bool post,QString *error) {
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
    QString token=ApiSession::token();
    if(!token.isEmpty())request.setRawHeader("Authorization",("Bearer "+token).toUtf8());
    QNetworkReply *reply=post?manager.post(request,QJsonDocument(body).toJson(QJsonDocument::Compact)):manager.get(request);
    QEventLoop loop;QTimer timeout;timeout.setSingleShot(true);
    QObject::connect(&timeout,&QTimer::timeout,reply,&QNetworkReply::abort);
    QObject::connect(reply,&QNetworkReply::finished,&loop,&QEventLoop::quit);
    timeout.start(8000);loop.exec();
    int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QJsonDocument document=QJsonDocument::fromJson(reply->readAll());
    if(reply->error()!=QNetworkReply::NoError || status<200 || status>=300 || !document.isObject()) {
        if(error) {
            if(status==401)*error="请先登录，或登录已过期";
            else if(status==403)*error="当前账号没有操作权限";
            else if(status>0)*error=QString("API请求失败（HTTP %1）").arg(status);
            else *error="服务端未连接或请求超时，请检查本机服务";
        }
        return {};
    }
    return document.object();
}
