#ifndef EVENTSERVICE_H
#define EVENTSERVICE_H
#include "../model/eventrepository.h"
#include <QCoreApplication>
#include <QDir>

// 业务层：把界面请求交给仓储。以后在这里扩展查询参数校验。
class EventService
{
public:
    EventResult queryLatest(const QString &configPath = QString())
    {
        QString path = configPath;
        if (path.isEmpty()) {
            QDir directory(QCoreApplication::applicationDirPath());
            path = directory.filePath("../NebulaVision/config/viewer.local.json");
        }
        return repository.queryLatest(path);
    }
private:
    EventRepository repository;
};
#endif
