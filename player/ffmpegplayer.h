#ifndef FFMPEGPLAYER_H
#define FFMPEGPLAYER_H
#include "videoplayer.h"
#include <QThread>
#include <QImage>
#include <QLabel>
#include <atomic>

// FFmpeg在独立子进程解码，工作线程负责读取固定大小的RGB帧。
class DecodeTask : public QThread
{
    Q_OBJECT
public:
    DecodeTask(const QString &source, const QString &executable, QObject *parent=nullptr);
    void requestStop();
    void setPaused(bool paused);
signals:
    void frameReady(const QImage &frame);
    void failed(const QString &message);
protected:
    void run() override;
private:
    QString source;
    QString executable;
    std::atomic<bool> stopping;
    std::atomic<bool> paused;
};

class FfmpegPlayer : public VideoPlayer
{
    Q_OBJECT
public:
    explicit FfmpegPlayer(QWidget *parent);
    ~FfmpegPlayer() override;
    QWidget *videoWidget() override;
    void open(const QString &source) override;
    void pause() override;
    void resume() override;
    void stop() override;
    static QString ffmpegExecutable();
signals:
    void frameDisplayed(); // 用于观测实际收到并显示的帧。
private:
    QLabel *surface;
    DecodeTask *task = nullptr;
};
#endif
