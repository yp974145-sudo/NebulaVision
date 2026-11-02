#ifndef FFMPEGPLAYER_H
#define FFMPEGPLAYER_H
#include "videoplayer.h"
#include <QThread>
#include <QImage>
#include <QLabel>
#include <atomic>
#include <QTimer>
#include <QMutex>

// FFmpeg在独立子进程解码，工作线程负责读取固定大小的RGB帧。
class DecodeTask : public QThread
{
    Q_OBJECT
public:
    DecodeTask(const QString &source, const QString &executable, QObject *parent=nullptr);
    void requestStop();
    void setPaused(bool paused);
    bool takeFrame(QImage *frame);
    int queuedFrames();
    int droppedFrames();
signals:
    void failed(const QString &message);
protected:
    void run() override;
private:
    QString source;
    QString executable;
    QMutex frameMutex;
    QImage latestFrame;
    bool hasFrame = false;
    int dropped = 0;
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
    int queuedFrames() const;
    int droppedFrames() const;
signals:
    void retryScheduled(int milliseconds);
    void frameDisplayed(); // 用于观测实际收到并显示的帧。
private:
    void startDecoding();
    void clearTask();
    QTimer retryTimer;
    QTimer renderTimer;
    bool userPaused = false;
    QString currentSource;
    bool networkSource = false;
    bool enabled = false;
    int retryCount = 0;
    int generation = 0;
    QLabel *surface;
    DecodeTask *task = nullptr;
};
#endif
