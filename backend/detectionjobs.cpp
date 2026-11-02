#include "detectionjobs.h"
#include "serverdb.h"
#include <QtConcurrent>
#include <QProcess>
#include <QProcessEnvironment>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFileInfo>
#include <QSemaphore>
#include <QDateTime>
static QSemaphore detectorSlots(2);
void DetectionJobs::start(const QJsonObject &config,const QString &id,const QString &recording,const QString &path,float threshold,const QJsonArray &roi) {
    Q_UNUSED(recording);
    (void)QtConcurrent::run([config,id,path,threshold,roi]() {
        bool acquired=detectorSlots.tryAcquire();
        ServerDb connection(config);QSqlQuery query(connection.db);
        query.prepare("UPDATE detection_jobs SET status='running' WHERE id=?");query.addBindValue(id);query.exec();
        QString error;QJsonObject result;
        if(!acquired)error="RESOURCE_EXHAUSTED";
        else {
            QProcess process;QProcessEnvironment environment=QProcessEnvironment::systemEnvironment();environment.insert("PYTHONPATH",config.value("python_packages").toString());environment.insert("PYTHONDONTWRITEBYTECODE","1");process.setProcessEnvironment(environment);
            process.setStandardErrorFile(QProcess::nullDevice());
            process.start(config.value("python").toString(),QStringList()<<config.value("rpc_client").toString());
            QJsonObject request{{"jobId","job-"+id},{"path",path},{"mode",config.value("detection_mode").toString("test")},
                {"threshold",double(threshold)},{"roi",roi},{"secret",config.value("rpc_secret").toString()},{"timeout",5}};
            if(process.waitForStarted(3000)) {
                process.write(QJsonDocument(request).toJson(QJsonDocument::Compact));process.closeWriteChannel();
                if(process.waitForFinished(8000)) {
                    QByteArray output=process.readAllStandardOutput();
                    if(output.size()>1024*1024)error="RESPONSE_TOO_LARGE";
                    else {
                        result=QJsonDocument::fromJson(output).object();
                        if(process.exitCode()!=0 || result.contains("error"))error=result.value("error").toString("RPC_FAILED");
                        else if(!result.value("detections").isArray())error="INVALID_RPC_RESPONSE";
                    }
                } else {process.kill();process.waitForFinished(1000);error="DEADLINE_EXCEEDED";}
            } else error="RPC_ADAPTER_UNAVAILABLE";
        }
        bool saved=false;
        if(error.isEmpty()) {
            // 不信任RPC返回的任意结构；真实模式必须与固定模型版本匹配。
            QJsonArray detections=result.value("detections").toArray();
            if(config.value("detection_mode").toString()=="onnx" &&
               (result.value("synthetic").toBool(true) || result.value("modelVersion").toString()!=config.value("model_version").toString()))error="MODEL_MISMATCH";
            for(const QJsonValue &value:detections){
                QJsonObject box=value.toObject();
                double confidence=box.value("confidence").toDouble(-1);
                QStringList coordinates={"x","y","width","height"};
                for(const QString &coordinate:coordinates){double value=box.value(coordinate).toDouble(-1);if(!qIsFinite(value)||value<0||value>1)error="INVALID_BOX";}
                if(box.value("x").toDouble()+box.value("width").toDouble()>1.001 || box.value("y").toDouble()+box.value("height").toDouble()>1.001)error="INVALID_BOX";
                if(!qIsFinite(confidence) || confidence<0 || confidence>1 || box.value("classId").toInt(-1)!=0)error="INVALID_DETECTION";
            }
            if(detections.size()>100)error="TOO_MANY_DETECTIONS";
            if(error.isEmpty() && connection.db.transaction()) {
                if(!result.value("synthetic").toBool() && !detections.isEmpty()) {
                    query.prepare("SELECT j.camera_id,j.recording_id,r.started_at FROM detection_jobs j JOIN recordings r ON r.id=j.recording_id WHERE j.id=? AND r.status='ready'");query.addBindValue(id);
                    if(!query.exec() || !query.next())error="EVIDENCE_UNAVAILABLE";
                    else {
                        QString camera=query.value(0).toString(),recordingId=query.value(1).toString();
                        QDateTime occurred=query.value(2).toDateTime().addMSecs(300);double confidence=0;
                        for(const QJsonValue &box:detections)confidence=qMax(confidence,box.toObject().value("confidence").toDouble());
                        // 同一摄像头同一录像同一规则最多一条事件，跨jobKey重试仍去重。
                        QString eventKey="person:"+camera+":"+recordingId;
                        query.prepare("INSERT IGNORE INTO events(event_key,camera_id,occurred_at,event_type,description,confidence,model_version,recording_id,status) VALUES (?,?,?,'person_in_region',?,?,?,?, 'new')");
                        query.addBindValue(eventKey);query.addBindValue(camera);query.addBindValue(occurred);
                        query.addBindValue(QString("YOLOv5 detected %1 person(s) in configured region; evidence recording %2").arg(detections.size()).arg(recordingId));query.addBindValue(confidence);query.addBindValue(result.value("modelVersion").toString());query.addBindValue(recordingId);
                        if(!query.exec())error="EVENT_INSERT_FAILED";
                        else {
                            result["deduplicated"]=query.numRowsAffected()==0;
                            query.prepare("SELECT id FROM events WHERE event_key=?");query.addBindValue(eventKey);
                            if(query.exec() && query.next())result["eventId"]=query.value(0).toString();else error="EVENT_LOOKUP_FAILED";
                        }
                    }
                }
                if(error.isEmpty()) {
                    query.prepare("UPDATE detection_jobs SET status='succeeded',result_json=?,error_code=NULL WHERE id=?");query.addBindValue(QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Compact)));query.addBindValue(id);
                    saved=query.exec() && connection.db.commit();
                    if(!saved)error="DB_COMMIT_FAILED";
                }
                if(!saved)connection.db.rollback();
            } else if(error.isEmpty())error="DB_TRANSACTION_FAILED";
        }
        if(!saved) {
            query.prepare("UPDATE detection_jobs SET status='failed',result_json=?,error_code=? WHERE id=?");query.addBindValue(QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Compact)));query.addBindValue(error);query.addBindValue(id);query.exec();
        }
        if(acquired)detectorSlots.release();
    });
}
