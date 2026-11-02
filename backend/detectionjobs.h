#ifndef DETECTIONJOBS_H
#define DETECTIONJOBS_H
#include <QJsonObject>
class DetectionJobs {
public:
    static void start(const QJsonObject &config,const QString &id,const QString &recording,const QString &path,float threshold,const QJsonArray &roi);
};
#endif
