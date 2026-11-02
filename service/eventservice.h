#ifndef EVENTSERVICE_H
#define EVENTSERVICE_H
#include "../model/eventrepository.h"
#include <QCoreApplication>
#include <QDir>

// 业务层：把界面请求交给仓储。以后在这里扩展查询参数校验。
class EventService
{
public:
    EventResult queryLatest(const QString &configPath = QString(), bool byDate = false,
                            const QDate &date = QDate(), int page = 1, int size = 100)
    {
        if (byDate && !date.isValid()) {
            EventResult result;
            result.error = "请选择有效日期";
            return result;
        }
        QString path = configPath;
        if (path.isEmpty()) {
            QDir directory(QCoreApplication::applicationDirPath());
            path = directory.filePath("../NebulaVision/config/viewer.local.json");
        }
        return repository.queryLatest(path, byDate, date, qMax(1, page), qBound(1, size, 100));
    }
private:
    EventRepository repository;
};
#endif
