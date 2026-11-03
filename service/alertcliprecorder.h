#ifndef ALERTCLIPRECORDER_H
#define ALERTCLIPRECORDER_H
#include <QObject>
#include <QImage>
#include <QList>
// 5帧/秒，保留最多3秒历史；报警后再收集5秒，只允许一条待保存录像。
class AlertClipRecorder:public QObject {
    Q_OBJECT
public:
    explicit AlertClipRecorder(QObject *parent=nullptr):QObject(parent){}
    void sample(const QImage &frame);
    void begin(const QString &eventKey);
    void finish();
    void retry();
    void resetHistory(){history.clear();}
signals:
    void statusChanged(const QString &text);
private:
    void upload();
    QList<QImage> history,frames;
    QString key,token,path;
    int before=0,after=0;
    bool collecting=false,busy=false;
};
#endif
