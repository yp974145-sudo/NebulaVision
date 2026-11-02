#ifndef REDISCLIENT_H
#define REDISCLIENT_H
#include <QStringList>
#include <QVariant>
// 小型RESP客户端，仅使用本轮所需命令；每次操作有总时限，在工作线程调用。
class RedisClient {
public:
    static QVariant command(int port,const QStringList &arguments,bool *ok);
};
#endif
