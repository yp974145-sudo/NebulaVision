#include "detectionjobs.h"
#include "serverdb.h"
#include <QtConcurrent>
#include <QProcess>
#include <QProcessEnvironment>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFileInfo>
#include <QSemaphore>
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
            QProcess process;QProcessEnvironment environment=QProcessEnvironment::systemEnvironment();environment.insert("PYTHONPATH",config.value("python_packages").toString());process.setProcessEnvironment(environment);
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
        query.prepare("UPDATE detection_jobs SET status=?,result_json=?,error_code=? WHERE id=?");
        query.addBindValue(error.isEmpty()?"succeeded":"failed");query.addBindValue(QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Compact)));query.addBindValue(error);query.addBindValue(id);query.exec();
        if(acquired)detectorSlots.release();
    });
}
