#ifndef LOGREDACTION_H
#define LOGREDACTION_H
#include <QtGlobal>
#include <QRegularExpression>
#include <QString>
#include <cstdio>
// Qt媒体日志可能带出完整RTSP授权地址；统一在写日志前去掉地址及认证信息。
inline void installLogRedaction(){
    qInstallMessageHandler([](QtMsgType type,const QMessageLogContext &context,const QString &message){
        QString safe=message;
        safe.replace(QRegularExpression("(?:rtsp|https?)://[^\\s'\"]+",QRegularExpression::CaseInsensitiveOption),"[URL已隐藏]");
        safe.replace(QRegularExpression("Bearer\\s+[^\\s]+",QRegularExpression::CaseInsensitiveOption),"Bearer [已隐藏]");
        QByteArray bytes=qFormatLogMessage(type,context,safe).toUtf8();fprintf(stderr,"%s\n",bytes.constData());
    });
}
#endif
