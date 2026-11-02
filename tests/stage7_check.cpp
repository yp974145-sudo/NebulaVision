#include <QApplication>
#include <QProcess>
#include <QElapsedTimer>
#include <QThread>
#include <functional>
#include "player/ffmpegplayer.h"
void spin7(int ms) { QElapsedTimer t;t.start();while(t.elapsed()<ms){QApplication::processEvents();QThread::msleep(10);} }
bool wait7(const std::function<bool()> &ready,int ms=12000) {QElapsedTimer t;t.start();while(t.elapsed()<ms){spin7(20);if(ready())return true;}return false;}
int main(int argc,char **argv) {
    QApplication app(argc,argv);
    QProcess server,publisher;
    QString url="rtsp://127.0.0.1:18554/demo";
    server.start(app.arguments().at(1),QStringList()<<app.arguments().at(2));
    QWidget widget;FfmpegPlayer player(&widget);
    int frames=0,retries=0;
    QObject::connect(&player,&FfmpegPlayer::frameDisplayed,[&frames](){++frames;});
    QObject::connect(&player,&FfmpegPlayer::retryScheduled,[&retries](int){++retries;});
    auto publish=[&](){publisher.start(FfmpegPlayer::ffmpegExecutable(),QStringList()<<"-hide_banner"<<"-loglevel"<<"error"<<"-nostdin"<<"-re"<<"-stream_loop"<<"-1"<<"-i"<<app.arguments().at(3)<<"-c"<<"copy"<<"-rtsp_transport"<<"tcp"<<"-f"<<"rtsp"<<url);};
    int result=[&]()->int {
        if(!server.waitForStarted(3000))return 1;
        spin7(500);publish();if(!publisher.waitForStarted(3000))return 2;spin7(1000);
        player.open(url);if(!wait7([&](){return frames>=5;}))return 3;
        publisher.kill();publisher.waitForFinished(2000);
        if(!wait7([&](){return retries>0;}))return 4;
        int before=frames;publish();if(!publisher.waitForStarted(3000))return 5;
        if(!wait7([&](){return frames>before+5;}))return 6;
        player.stop();int stopped=retries;int oldFrames=frames;spin7(1800);
        if(retries!=stopped || frames!=oldFrames)return 7;
        player.open("rtsp://127.0.0.1:18555/not-running");
        if(!wait7([&](){return retries>stopped;}))return 8;
        player.stop();stopped=retries;spin7(2200);
        if(retries!=stopped)return 9;
        return 0;
    }();
    player.stop();publisher.kill();publisher.waitForFinished(2000);server.kill();server.waitForFinished(2000);
    return result;
}
