#include "recorder.h"
#include "serverdb.h"
#include <QtConcurrent>
#include <QSemaphore>
#include <QProcess>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QStorageInfo>
static QSemaphore recordingSlots(2);
void Recorder::start(const QJsonObject &config,const QString &id,const QString &filename,const QString &url,int seconds) {
    // 请求立即返回202，录制在独立工作任务执行。最多同时两段。
    (void)QtConcurrent::run([config,id,filename,url,seconds]() {
        bool slot=recordingSlots.tryAcquire();
        bool ready=false;
        QString root=config.value("recording_root").toString();
        QString partial=QDir(root).filePath(filename+".part.mp4");
        QString finalPath=QDir(root).filePath(filename);
        if(slot && !root.isEmpty() && QDir().mkpath(root)) {
            QStorageInfo storage(root);storage.refresh();
            if(storage.bytesAvailable()>32*1024*1024) {
                QProcess process;
                process.setStandardOutputFile(QProcess::nullDevice());
                process.setStandardErrorFile(QProcess::nullDevice());
                QString ffmpeg=config.value("ffmpeg").toString();
                process.start(ffmpeg,QStringList()<<"-hide_banner"<<"-loglevel"<<"error"<<"-nostdin"<<"-y"<<"-rtsp_transport"<<"tcp"<<"-timeout"<<"3000000"<<"-i"<<url<<"-t"<<QString::number(seconds)<<"-an"<<"-vf"<<"setpts=PTS-STARTPTS"<<"-c:v"<<"libx264"<<"-preset"<<"veryfast"<<"-pix_fmt"<<"yuv420p"<<"-g"<<"30"<<"-movflags"<<"+faststart"<<partial);
                bool completed=process.waitForStarted(3000) && process.waitForFinished((seconds+10)*1000);
                if(!completed){process.kill();process.waitForFinished(2000);}
                if(completed && process.exitCode()==0 && QFileInfo(partial).size()>0) {
                    // 验证封片能解码出视频，而非仅看文件非空。
                    process.start(ffmpeg,QStringList()<<"-hide_banner"<<"-loglevel"<<"error"<<"-i"<<partial<<"-frames:v"<<"1"<<"-f"<<"null"<<"-");
                    bool valid=process.waitForStarted(3000) && process.waitForFinished(5000) && process.exitCode()==0;
                    if(!valid){process.kill();process.waitForFinished(1000);}
                    if(valid)ready=QFile::rename(partial,finalPath);
                }
            }
        }
        ServerDb connection(config);QSqlQuery query(connection.db);
        query.prepare("UPDATE recordings SET status=?,ended_at=UTC_TIMESTAMP(3),bytes=? WHERE id=? AND status='writing'");
        query.addBindValue(ready?"ready":"failed");query.addBindValue(ready?QFileInfo(finalPath).size():0);query.addBindValue(id);query.exec();
        // DB更新失败时保留文件供对账，不谎报ready，也不删除其他录像。
        if(slot)recordingSlots.release();
    });
}
