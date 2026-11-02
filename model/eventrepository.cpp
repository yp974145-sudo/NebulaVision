#include "eventrepository.h"
#include "../util/devdbconnection.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDateTime>

EventResult EventRepository::queryLatest(const QString &configPath)
{
    EventResult result;
    // 先创建连接管理对象，后创建query；返回时query会先销毁。
    DevDbConnection connection;
    QString error;
    if (connection.open(configPath, &error) == false) {
        result.error = "新库连接失败：" + error;
        return result;
    }

    QSqlQuery query(connection.database());
    // JOIN通过camera_id找到摄像头名称；LEFT JOIN保留未匹配到摄像头的事件。
    // 阶段2仅展示最近100条，日期筛选和分页留到阶段3。
    QString sql = "SELECT e.id, c.name, e.occurred_at, e.event_type, "
                  "e.description, e.status "
                  "FROM events e LEFT JOIN cameras c ON c.id=e.camera_id "
                  "ORDER BY e.occurred_at DESC, e.id DESC LIMIT 100";
    if (query.exec(sql) == false) {
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
