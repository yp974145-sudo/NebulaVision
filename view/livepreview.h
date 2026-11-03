#ifndef LIVEPREVIEW_H
#define LIVEPREVIEW_H
#include <QWidget>
#include <QImage>
#include <QProcess>
#include <QTimer>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QCameraDevice>
#include <QPointer>
#include <QJsonObject>
class QPushButton;
#include "../player/ffmpegplayer.h"
class QCamera;
class QMediaPlayer;
class QVideoSink;
// 一次只送一帧，显示与该次推理完全对应的画面，忙时丢弃旧预览帧。
class LivePreview:public QWidget {
    Q_OBJECT
public:
    explicit LivePreview(QWidget *parent=nullptr);
    ~LivePreview() override;
    void setCameraId(const QString &id);
    QString lastSavedEvent;
    void openCamera(const QCameraDevice &device);
    void openStream(const QString &url);
    void stop();
    void pause();
    void resume();
    int processedFrames=0;
    int lastPeople=0;
    int lastFaces=0;
    bool gestureAlert=false;
    double lastGestureScore=0;
protected:
    void closeEvent(QCloseEvent *event) override;
private:
    void startWorker();
    void sendFrame();
    void receive();
    void saveAlert();
    QLabel *video;
    QLabel *status;
    QDoubleSpinBox *threshold;
    QCheckBox *alarm;
    QProcess worker;
    QTimer sample,deadline;
    QVideoSink *sink;
    QCamera *camera=nullptr;
    QPointer<DecodeTask> decodeTask;
    QImage latest,pending;
    QByteArray buffer;
    bool ready=false,busy=false;
    int positive=0,negative=0;
    bool alarming=false;
    quint64 frameVersion=0,sentVersion=0;
    QString cameraId;
    QJsonObject pendingAlert;
    QLabel *savedStatus;
    QPushButton *retrySave;
    bool saving=false;
    bool paused=false;
    QString lastUrl;
};
#endif
