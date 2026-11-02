#ifndef MONITORWIDGET_H
#define MONITORWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedLayout>
#include <QGridLayout>
#include <QListWidget>
#include <QListWidgetItem>
#include <QIcon>
#include <QFile>
#include <QPushButton>
#include "../player/videoplayer.h"

class MonitorWidget : public QWidget
{
    Q_OBJECT
private:
    VideoPlayer *player;
    QLabel *playStatus;
    QStackedLayout *video_layout;
    QWidget *left_widget;
    QWidget *right_widget;
    QPushButton *btn_channel1;
    QPushButton *btn_channel4;//单\四通道
    QLabel *lab_vidoe,*lab_videos[4];
public:
    explicit MonitorWidget(QWidget *parent = nullptr);
    void init_leftWidget();
    void init_qss();
    void init_rightwidget();
    void init_connect();
signals:

public slots:
    void switch_to_channel1();
    void switch_to_channel4();

};

#endif // MONITORWIDGET_H
