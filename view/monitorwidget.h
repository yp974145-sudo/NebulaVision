#ifndef MONITORWIDGET_H
#define MONITORWIDGET_H
#include <QWidget>
#include <QGridLayout>
#include <QLabel>
#include <QListWidget>
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
public slots:
    void switch_to_channel1();
    void switch_to_channel4();
private:
    void updateLayout();
    QGridLayout *grid;
    QListWidget *channelList;
    QListWidget *deviceList;
    QLabel *deviceStatus;
    QWidget *tiles[4];
    FfmpegPlayer *players[4];
    int selectedChannel = 0;
    bool fourViews = false;
};
#endif
