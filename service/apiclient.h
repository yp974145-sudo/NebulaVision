#ifndef APICLIENT_H
#define APICLIENT_H
#include <QJsonObject>
#include <QString>
// 登录令牌只保存在本进程内存，不写文件。后台查询会复制当前令牌。
class ApiSession {
public:
    static QString token();
    static void setToken(const QString &token);
};
class ApiClient {
public:
    // 该函数供工作线程调用；网络事件异步推进，最长8秒，不能在GUI中直接等待。
    static bool download(const QString &path,const QString &target,QString *error);
    static QJsonObject request(const QString &path,const QJsonObject &body, bool post, QString *error,const QString &tokenOverride=QString());
};
#endif
