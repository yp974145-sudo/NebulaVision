#include <functional>
#include <QApplication>
#include <QMediaPlayer>
#include <QVideoProbe>
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
int main(int argc,char **argv) {
    QApplication app(argc,argv);
    QWidget window;
    QtVideoPlayer player(&window);
    QMediaPlayer *media=player.findChild<QMediaPlayer *>();
    int frames=0;
    QVideoProbe probe;
    probe.setSource(media);
    QObject::connect(&probe,&QVideoProbe::videoFrameProbed,[&frames](const QVideoFrame &){++frames;});
    player.open(app.arguments().at(1));
    if(!spinUntil([&](){return frames>2;})) return 1;
    player.pause();
    if(!spinUntil([&](){return media->state()==QMediaPlayer::PausedState;})) return 2;
    player.resume();
    if(!spinUntil([&](){return media->state()==QMediaPlayer::PlayingState;})) return 3;
    player.stop();
    if(media->state()!=QMediaPlayer::StoppedState) return 4;
    player.open(app.arguments().at(1));
    int before=frames;
    if(!spinUntil([&](){return frames>before+2;})) return 5;
    player.stop();
    bool error=false;
    QObject::connect(&player,&VideoPlayer::playbackError,[&error](const QString &){error=true;});
    player.open("missing-stage5-file.mp4");
    if(!error) return 6;
    return 0;
}
