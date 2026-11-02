#include <QApplication>
#include <QElapsedTimer>
#include <QThread>
#include "player/ffmpegplayer.h"
void spin(int ms) { QElapsedTimer t;t.start();while(t.elapsed()<ms){QApplication::processEvents();QThread::msleep(10);} }
int main(int argc,char **argv) {
    QApplication app(argc,argv);
    QWidget widget;
    FfmpegPlayer player(&widget);
    int frames=0;
    QObject::connect(&player,&FfmpegPlayer::frameDisplayed,[&frames](){++frames;});
    player.open(app.arguments().at(1));
    spin(1600);
    if(frames<5) return 1;
    player.pause();spin(200);int pausedFrames=frames;spin(400);
    if(frames!=pausedFrames) return 2;
    player.resume();spin(500);
    if(frames<=pausedFrames) return 3;
    player.stop();int stoppedFrames=frames;spin(200);
    if(frames!=stoppedFrames) return 4;
    player.open(app.arguments().at(1));spin(1800);
    if(frames<=stoppedFrames) return 5;
    player.stop();
    bool error=false;
    QObject::connect(&player,&VideoPlayer::playbackError,[&error](const QString &){error=true;});
    player.open("missing-stage6.mp4");
    if(!error) return 6;
    return 0;
}
