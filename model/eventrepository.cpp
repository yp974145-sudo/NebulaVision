#include "eventrepository.h"
#include "../util/devdbconnection.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDateTime>

EventResult EventRepository::queryLatest(const QString &configPath, bool byDate,
                                         const QDate &date, int page, int size)
{
    EventResult result;
    // 先创建连接管理对象，后创建query；返回时query会先销毁。
    DevDbConnection connection;
    QString error;
    if (connection.open(configPath, &error) == false) {
        result.error = "新库连接失败：" + error;
        return result;
    }

    // 所有分页参数由Service校验；仓储再次限制，避免直接调用时除零。
    size = qBound(1, size, 100);
    QString from = " FROM events e LEFT JOIN cameras c ON c.id=e.camera_id";
    QString where;
    if (byDate) {
        where = " WHERE e.occurred_at>=:begin AND e.occurred_at<:end";
    }
    // 半开区间：包含当天零点，不包含次日零点，保留当天末尾的毫秒。
    auto bindDate = [byDate, date](QSqlQuery &query) {
        if (byDate) {
            query.bindValue(":begin", date.toString("yyyy-MM-dd") + " 00:00:00");
            query.bindValue(":end", date.addDays(1).toString("yyyy-MM-dd") + " 00:00:00");
        }
    };
    QSqlQuery count(connection.database());
    count.prepare("SELECT COUNT(*)" + from + where);
    bindDate(count);
    if (!count.exec() || !count.next()) {
        result.error = "事件数量统计失败。";
        return result;
    }
    result.total = count.value(0).toInt();
    result.pages = qMax(1, (result.total + size - 1) / size);
    result.page = qBound(1, page, result.pages);
    count.finish();

    QSqlQuery query(connection.database());
    QString sql = "SELECT e.id, c.name, e.occurred_at, e.event_type, "
                  "e.description, e.status" + from + where +
                  " ORDER BY e.occurred_at DESC,e.id DESC LIMIT :size OFFSET :offset";
    query.prepare(sql);
    bindDate(query);
    query.bindValue(":size", size);
    query.bindValue(":offset", (result.page - 1) * size);
    if (!query.exec()) {
        result.error = "事件查询失败，请检查新库表结构和账号查询权限。";
        return result;
    }
    while (query.next()) {
        EventData event;
        // 列号从0开始，与上面SELECT字段顺序一致。
        event.id = query.value(0).toString();
        event.cameraName = query.value(1).toString();
        // 这里按项目约定把存储时间当作UTC显示，不进行本地时区转换。
        event.occurredAt = query.value(2).toDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
        event.type = query.value(3).toString();
        event.description = query.value(4).toString();
        event.status = query.value(5).toString();
        result.events.append(event);
    }
    if (query.lastError().isValid()) {
        result.events.clear();
        result.error = "读取事件结果失败，请重试。";
    }
    return result;
}
