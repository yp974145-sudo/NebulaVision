#ifndef RECORDMODEL_H
#define RECORDMODEL_H
#include <QDate>
#include <QStringList>
#include <QVector>
struct RecordPage {
    QStringList headers;
    QVector<QStringList> rows;
    QVector<QString> recordingIds; // 事件关联证据，供界面隐藏元数据使用。
    int total = 0;
    int page = 1;
    int pages = 1;
    QString error;
};
class RecordModel {
public:
    RecordPage query(bool events, bool byDate, const QDate &date, int page, int pageSize);
};
#endif
