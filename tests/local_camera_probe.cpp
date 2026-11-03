#include <QGuiApplication>
#include <QCamera>
#include <QMediaDevices>
#include <QMediaCaptureSession>
#include <QVideoSink>
#include <QVideoFrame>
#include <QTimer>
#include <cstdio>
int main(int argc,char **argv) {
    QGuiApplication app(argc,argv);auto devices=QMediaDevices::videoInputs();
    fprintf(stdout,"Detected cameras: %lld\n",static_cast<long long>(devices.size()));
    for(const auto &device:devices)fprintf(stdout,"%s\n",device.description().toUtf8().constData());
    if(!app.arguments().contains("--capture"))return 0;
    if(devices.isEmpty())return 2;
    QCamera camera(devices.first());QMediaCaptureSession session;QVideoSink sink;
    session.setCamera(&camera);session.setVideoSink(&sink);bool frame=false;
    QObject::connect(&sink,&QVideoSink::videoFrameChanged,&app,[&](const QVideoFrame &value){
        if(value.isValid()){frame=true;camera.stop();app.quit();}
    });
    QObject::connect(&camera,&QCamera::errorOccurred,&app,[&](QCamera::Error,const QString &message){fprintf(stdout,"Camera error: %s\n",message.toUtf8().constData());app.quit();});
    QTimer::singleShot(10000,&app,&QCoreApplication::quit);camera.start();app.exec();camera.stop();
    fprintf(stdout,"Received real camera frame: %s\n",frame?"yes":"no");return frame?0:3;
}
