#ifndef RECORDSERVICE_H
#define RECORDSERVICE_H
#include "../model/recordmodel.h"
#include "apiclient.h"
#include <QUrlQuery>
#include <QJsonArray>
class RecordService {
public:
    RecordPage query(bool events,bool byDate,const QDate &date,int page,int size) {
        Q_UNUSED(events);
        RecordPage result;
        result.headers=QStringList()<<"编号"<<"摄像头"<<"开始时间 UTC"<<"结束时间 UTC"<<"大小 MB"<<"状态";
        if(byDate && !date.isValid()){result.error="请选择有效日期";return result;}
        QUrlQuery params;params.addQueryItem("page",QString::number(qMax(1,page)));params.addQueryItem("size",QString::number(qBound(1,size,100)));
        if(byDate)params.addQueryItem("date",date.toString(Qt::ISODate));
        QJsonObject response=ApiClient::request("/api/recordings?"+params.toString(QUrl::FullyEncoded),{},false,&result.error);
        if(!result.error.isEmpty())return result;
        result.total=response.value("total").toInt();result.page=response.value("page").toInt(1);result.pages=response.value("pages").toInt(1);
        QJsonArray rows=response.value("recordings").toArray();
        for(int i=0;i<rows.size();i++){
            QJsonObject row=rows[i].toObject();
            result.rows.append(QStringList()<<row.value("id").toString()<<row.value("cameraName").toString()<<row.value("startedAt").toString()<<row.value("endedAt").toString()<<QString::number(row.value("bytes").toDouble()/1048576.0,'f',2)<<row.value("status").toString());
        }
        return result;
    }
};
#endif
