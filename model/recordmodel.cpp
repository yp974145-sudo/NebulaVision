#include "recordmodel.h"
#include "../util/dbconn.h"
#include <QDateTime>
// 录像和异常事件共用分页逻辑，通过 events 决定查哪张表。
RecordPage RecordModel::query(bool events, bool byDate, const QDate &date, int page, int pageSize)
{
    RecordPage result;
    result.headers = events ? QStringList({"编号","通道","事件时间","事件描述","关联视频"})
                            : QStringList({"编号","视频文件名","通道","开始时间","结束时间","大小(MB)","存储路径"});
    QSqlDatabase db = DbConn::getInstence()->getDb();
    if (!db.isOpen() && !db.open()) { result.error = "数据库连接失败：" + db.lastError().text(); return result; }
    // 关联通道表是为了显示摄像头名称；通道缺失时，也保留原来的记录。
    // chennel_id 是现有数据库里的字段名，这里按实际拼写使用。
    QString from = events ? " from exception_log e left join camera_channel c on e.chennel_id=c.id"
                          : " from video v left join camera_channel c on v.channel_id=c.id";
    QString time = events ? "e.event_time" : "v.start_time";
    QString where = byDate ? " where " + time + ">=:begin and " + time + "<:end" : "";
    auto bindDate = [&](QSqlQuery &query) {
        // 查当天零点到次日零点之前，最后一秒里带毫秒的记录也不会漏掉。
        if (byDate) {
            query.bindValue(":begin", date.toString("yyyy-MM-dd") + " 00:00:00");
            query.bindValue(":end", date.addDays(1).toString("yyyy-MM-dd") + " 00:00:00");
        }
    };
    // 先查总条数，才能算出有几页。统计和列表要使用相同的筛选条件。
    QSqlQuery count(db);
    count.prepare("select count(*)" + from + where);
    bindDate(count);
    if (!count.exec() || !count.next()) { result.error = "统计失败：" + count.lastError().text(); return result; }
    result.total = count.value(0).toInt();
    count.finish();
    // 例如 25 条、每页 10 条，就是 3 页；没有记录时页码仍显示为 1。
    result.pages = qMax(1, (result.total + pageSize - 1) / pageSize);
    // 查询条件变化后，原来的页码可能超出范围，这里把它调回有效页。
    result.page = qBound(1, page, result.pages);
    QString fields = events ? "e.id,c.camera_name,e.event_time,e.event_desc,e.related_video_path"
                            : "v.id,v.video_name,c.camera_name,v.start_time,v.end_time,v.file_size,v.video_path";
    QString id = events ? "e.id" : "v.id";
    QSqlQuery query(db);
    // 时间相同时再按编号排序，保证翻页顺序稳定。
    // LIMIT 只取当前页，OFFSET 跳过前面的记录，不用把全表加载到界面。
    query.prepare("select " + fields + from + where + " order by " + time + " desc," + id +
                  QString(" desc limit %1 offset %2").arg(pageSize).arg((result.page - 1) * pageSize));
    bindDate(query);
    if (!query.exec()) { result.error = "查询失败：" + query.lastError().text(); return result; }
    // 把查询结果整理成表格要显示的内容，界面收到后直接填表。
    while (query.next()) {
        QStringList row;
        for (int i=0; i<result.headers.size(); ++i) {
            if ((events && i==2) || (!events && (i==3 || i==4)))
                row << query.value(i).toDateTime().toString("yyyy-MM-dd HH:mm:ss");
            else if (!events && i==5)
                row << QString::number(query.value(i).toLongLong()/1048576.0, 'f', 2);
            else row << query.value(i).toString();
        }
        // 数据库没填视频名称时，从路径里取文件名。
        if (!events && row[1].isEmpty()) row[1] = row.last().section('/',-1).section('\\',-1);
        result.rows.append(row);
    }
    return result;
}
