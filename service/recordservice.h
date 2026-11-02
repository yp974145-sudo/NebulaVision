#ifndef RECORDSERVICE_H
#define RECORDSERVICE_H
#include "../model/recordmodel.h"
class RecordService {
public:
    RecordPage query(bool events, bool byDate, const QDate &date, int page, int size) {
        // 先检查日期，再限制页码和每页条数，避免无效参数传到 Model。
        if (byDate && !date.isValid()) { RecordPage r; r.error="请选择有效日期"; return r; }
        return model.query(events, byDate, date, qMax(1,page), qBound(1,size,100));
    }
private:
    RecordModel model;
};
#endif
