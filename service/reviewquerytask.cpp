#include "reviewquerytask.h"
#include "eventservice.h"
#include "recordservice.h"
void ReviewQueryTask::run(){
    RecordPage pageResult;
    if(events){
        EventService service;EventResult result=service.queryLatest(QString(),byDate,date,page,size);
        pageResult.headers=QStringList()<<"编号"<<"摄像头"<<"时间 UTC"<<"类型"<<"描述"<<"状态";
        pageResult.error=result.error;pageResult.total=result.total;pageResult.page=result.page;pageResult.pages=result.pages;
        for(const EventData &event:result.events){pageResult.rows.append(QStringList()<<event.id<<event.cameraName<<event.occurredAt<<event.type<<event.description<<event.status);pageResult.recordingIds.append(event.recordingId);}
    }else{
        RecordService service;pageResult=service.query(false,byDate,date,page,size);
    }
    emit resultReady(requestId,pageResult);
}
