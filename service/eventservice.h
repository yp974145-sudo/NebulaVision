#ifndef EVENTSERVICE_H
#define EVENTSERVICE_H
#include "../model/eventrepository.h"
#include "apiclient.h"
#include <QUrlQuery>
#include <QJsonArray>

class EventService {
public:
    EventResult queryLatest(const QString &configPath=QString(),bool byDate=false,
                            const QDate &date=QDate(),int page=1,int size=100) {
        Q_UNUSED(configPath); // 保留旧调用签名；查询不再读取本地数据库密码。
        EventResult result;
        if(byDate && !date.isValid()){result.error="请选择有效日期";return result;}
        QUrlQuery query;
        query.addQueryItem("page",QString::number(qMax(1,page)));
        query.addQueryItem("size",QString::number(qBound(1,size,100)));
        if(byDate)query.addQueryItem("date",date.toString(Qt::ISODate));
        QJsonObject response=ApiClient::request("/api/events?"+query.toString(QUrl::FullyEncoded),{},false,&result.error);
        if(!result.error.isEmpty())return result;
        if(!response.value("events").isArray()){result.error="服务端事件格式错误";return result;}
        result.total=response.value("total").toInt();result.page=response.value("page").toInt(1);result.pages=response.value("pages").toInt(1);
        QJsonArray rows=response.value("events").toArray();
        for(int i=0;i<rows.size();i++) {
            QJsonObject row=rows.at(i).toObject();EventData event;
            event.id=row.value("id").toString();event.cameraName=row.value("cameraName").toString();
            event.occurredAt=row.value("occurredAt").toString();event.type=row.value("type").toString();
            event.description=row.value("description").toString();event.status=row.value("status").toString();
            result.events.append(event);
        }
        return result;
    }
};
#endif
