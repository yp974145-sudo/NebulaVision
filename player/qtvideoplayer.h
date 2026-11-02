#ifndef QTVIDEOPLAYER_H
#define QTVIDEOPLAYER_H
#include "videoplayer.h"
#include <QMediaPlayer>
#include <QVideoWidget>
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
#include <QAudioOutput>
#endif
class QtVideoPlayer : public VideoPlayer
{
    Q_OBJECT
public:
    explicit QtVideoPlayer(QWidget *parent);
    QWidget *videoWidget() override;
    void open(const QString &source) override;
    void pause() override;
    void resume() override;
    void stop() override;
private:
    QMediaPlayer *media;
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    QAudioOutput *audio;
#endif
    QVideoWidget *surface;
};
#endif
