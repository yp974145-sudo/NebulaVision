#ifndef MONITORWIDGET_H
#define MONITORWIDGET_H
#include <QWidget>
#include <QGridLayout>
#include <QLabel>
#include <QListWidget>
#include <QComboBox>
#include <QPointer>
#include <QJsonObject>
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
#include "livepreview.h"
#endif
#include "../player/ffmpegplayer.h"
class MonitorWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MonitorWidget(QWidget *parent = nullptr);
    ~MonitorWidget() override;
    void openSource(int channel, const QString &source);
    void stopAll();
    void refreshDevices();
    void openDevice(const QJsonObject &device);
    void stopChannel(int channel);
public slots:
    void switch_to_channel1();
    void switch_to_channel4();
private:
    void updateLayout();
    QGridLayout *grid;
    QComboBox *channelList;
    QListWidget *deviceList;
    QLabel *deviceStatus;
    QWidget *tiles[4];
    FfmpegPlayer *players[4];
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    QPointer<LivePreview> liveViews[4];
    void embedLive(int channel,LivePreview *view);
#endif
    int selectedChannel = 0;
    bool fourViews = false;
    int openGeneration[4]={0,0,0,0};
};
#endif
