#ifndef QTVIDEOPLAYER_H
#define QTVIDEOPLAYER_H
#include "videoplayer.h"
#include <QMediaPlayer>
#include <QVideoWidget>
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
    QVideoWidget *surface;
};
#endif
