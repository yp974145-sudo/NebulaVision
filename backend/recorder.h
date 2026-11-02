#ifndef RECORDER_H
#define RECORDER_H
#include <QJsonObject>
class Recorder {
public:
    static void start(const QJsonObject &config,const QString &id,const QString &filename,const QString &url,int seconds);
};
#endif
