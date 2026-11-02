#ifndef VIDEOPLAYER_H
#define VIDEOPLAYER_H
#include <QObject>
#include <QWidget>
// 统一播放器接口：界面只调用这些操作，不关心底层是Qt还是FFmpeg。
class VideoPlayer : public QObject
{
    Q_OBJECT
public:
    explicit VideoPlayer(QObject *parent=nullptr) : QObject(parent) {}
    virtual QWidget *videoWidget() = 0;
    virtual void open(const QString &source) = 0;
    virtual void pause() = 0;
    virtual void resume() = 0;
    virtual void stop() = 0;
signals:
    void statusChanged(const QString &status);
    void playbackError(const QString &message);
};
#endif
