#ifndef MEDIAADAPTER_H
#define MEDIAADAPTER_H
#include <QJsonObject>
// 媒体控制边界：本轮部署MediaMTX，通过HTTP鉴权回调验证签名令牌。
class MediaAdapter {
public:
    static QString sign(const QJsonObject &claims,const QString &secret);
    static QJsonObject verify(const QString &token,const QString &secret);
};
#endif
