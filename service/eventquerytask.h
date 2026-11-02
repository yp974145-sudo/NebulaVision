#ifndef EVENTQUERYTASK_H
#define EVENTQUERYTASK_H
#include <QThread>
#include "../model/eventrepository.h"
#include <QMetaType>
Q_DECLARE_METATYPE(EventResult)

// 只负责后台查询，不接触任何窗口或控件。
class EventQueryTask : public QThread
{
    Q_OBJECT
public:
    EventQueryTask(const QString &path, bool byDate, const QDate &date,
                   int page, int size, int requestId, QObject *parent = nullptr);
    int requestId;
signals:
    void resultReady(int requestId, EventResult result);
protected:
    void run() override;
private:
    QString path;
    bool byDate;
    QDate date;
    int page;
    int size;
};
#endif
