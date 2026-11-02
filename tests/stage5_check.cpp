#include <functional>
#include <QApplication>
#include <QMediaPlayer>
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
#include <QVideoSink>
#include <QVideoFrame>
#else
#include <QVideoProbe>
#endif
#include <QElapsedTimer>
#include <QThread>
#include "player/qtvideoplayer.h"
bool spinUntil(const std::function<bool()> &ready) {
    QElapsedTimer time; time.start();
    while(time.elapsed()<6000) {
        QApplication::processEvents();
        if(ready()) return true;
        QThread::msleep(10);
    }
    return false;
}
int mediaState(QMediaPlayer *media) {
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    return media->playbackState();
#else
    return media->state();
#endif
}
int main(int argc,char **argv) {
    QApplication app(argc,argv);
    QWidget window;
    QtVideoPlayer player(&window);
    QMediaPlayer *media=player.findChild<QMediaPlayer *>();
    int frames=0;
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    QVideoSink *sink = qobject_cast<QVideoWidget *>(player.videoWidget())->videoSink();
    QObject::connect(sink,&QVideoSink::videoFrameChanged,[&frames](const QVideoFrame &frame){if(frame.isValid())++frames;});
#else
    QVideoProbe probe;
    probe.setSource(media);
    QObject::connect(&probe,&QVideoProbe::videoFrameProbed,[&frames](const QVideoFrame &){++frames;});
#endif
    player.open(app.arguments().at(1));
    if(!spinUntil([&](){return frames>2;})) return 1;
    player.pause();
    if(!spinUntil([&](){return mediaState(media)==QMediaPlayer::PausedState;})) return 2;
    player.resume();
    if(!spinUntil([&](){return mediaState(media)==QMediaPlayer::PlayingState;})) return 3;
    player.stop();
    if(mediaState(media)!=QMediaPlayer::StoppedState) return 4;
    player.open(app.arguments().at(1));
    int before=frames;
    if(!spinUntil([&](){return frames>before+2;})) return 5;
    player.stop();
    bool error=false;
    QObject::connect(&player,&VideoPlayer::playbackError,[&error](const QString &){error=true;});
    player.open("missing-stage5-file.mp4");
    if(!error) return 6;
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    probe.setSource(static_cast<QMediaObject *>(nullptr));
#endif
    return 0;
}
