#ifndef EVENTREPOSITORY_H
#define EVENTREPOSITORY_H
#include <QString>
#include <QVector>
#include <QDate>

// DTO：只保存一条事件的数据，不操作数据库，也不操作界面。
struct EventData
{
    QString id;
    QString cameraName;
    QString recordingId;
    QString occurredAt;
    QString type;
    QString description;
    QString status;
};

struct EventResult
{
    QVector<EventData> events;
    QString error;
    int total = 0;
    int page = 1;
    int pages = 1;
};

// 仓储：负责执行SQL，把数据库结果转换成事件数据。
class EventRepository
{
public:
    EventResult queryLatest(const QString &configPath, bool byDate = false,
                            const QDate &date = QDate(), int page = 1, int size = 100);
};
#endif
