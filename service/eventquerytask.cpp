#include "eventquerytask.h"
#include "eventservice.h"
EventQueryTask::EventQueryTask(const QString &path, bool byDate, const QDate &date,
                             int page, int size, int requestId, QObject *parent)
    : QThread(parent)
{
    this->path = path;
    this->byDate = byDate;
    this->date = date;
    this->page = page;
    this->size = size;
    this->requestId = requestId;
}
void EventQueryTask::run()
{
    // 连接在工作线程中创建、查询、释放，不传GUI线程的数据库句柄。
    EventService service;
    EventResult result = service.queryLatest(path, byDate, date, page, size);
    emit resultReady(requestId, result);
}
