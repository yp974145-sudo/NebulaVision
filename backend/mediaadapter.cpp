#include "mediaadapter.h"
#include <QMessageAuthenticationCode>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QDateTime>
QString MediaAdapter::sign(const QJsonObject &claims,const QString &secret) {
    QByteArray payload=QJsonDocument(claims).toJson(QJsonDocument::Compact).toBase64(QByteArray::Base64UrlEncoding|QByteArray::OmitTrailingEquals);
    QByteArray signature=QMessageAuthenticationCode::hash(payload,secret.toUtf8(),QCryptographicHash::Sha256).toBase64(QByteArray::Base64UrlEncoding|QByteArray::OmitTrailingEquals);
    return QString::fromLatin1(payload+'.'+signature);
}
QJsonObject MediaAdapter::verify(const QString &token,const QString &secret) {
    QList<QByteArray> parts=token.toLatin1().split('.');if(parts.size()!=2 || token.size()>2048 || secret.size()<32)return {};
    QByteArray expected=QMessageAuthenticationCode::hash(parts[0],secret.toUtf8(),QCryptographicHash::Sha256).toBase64(QByteArray::Base64UrlEncoding|QByteArray::OmitTrailingEquals);
    unsigned difference=unsigned(expected.size()^parts[1].size());
    for(int i=0;i<qMin(expected.size(),parts[1].size());i++)difference|=unsigned(uchar(expected[i])^uchar(parts[1][i]));
    if(difference)return {};
    QJsonObject claims=QJsonDocument::fromJson(QByteArray::fromBase64(parts[0],QByteArray::Base64UrlEncoding)).object();
    if(claims.value("expires").toDouble()<=QDateTime::currentSecsSinceEpoch())return {};
    return claims;
}
