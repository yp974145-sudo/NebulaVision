#include "monitorwidget.h"
#include "../player/qtvideoplayer.h"
#include <QFileDialog>


MonitorWidget::MonitorWidget(QWidget *parent) : QWidget(parent)
{
    //设置监控窗口主布局为横向布局
    QHBoxLayout *main_layout=new QHBoxLayout;
    this->setLayout(main_layout);

    //创建左右两个盒子，放入主布局容器中，左侧窗口站1，右侧窗口6
    left_widget=new QWidget;
    left_widget->setObjectName("left_box");
    main_layout->addWidget(left_widget,1);
    right_widget=new QWidget;
    main_layout->addWidget(right_widget,6);

    //初始化左窗口
    this->init_leftWidget();

    this->init_qss();
    this->init_rightwidget();
    this->init_connect();
}

void MonitorWidget::init_leftWidget()
{
     //设置左窗口对象名，用于样式设置
    QVBoxLayout *left_layout=new QVBoxLayout;
    left_widget->setLayout(left_layout);

    QLabel *lab_devicelist=new QLabel("设备列表");
    lab_devicelist->setObjectName("device_list");
    lab_devicelist->setAlignment(Qt::AlignCenter);
    left_layout->addWidget(lab_devicelist);

    //创建列表容器，用于展示设备列表
    QListWidget *device_list=new QListWidget;
    left_layout ->addWidget(device_list);
    device_list->addItem(new QListWidgetItem(QIcon(":/img/camera.png"),"xxxxxxxxxx"));
    device_list->addItem(new QListWidgetItem(QIcon(":/img/camera.png"),"zzzzzzzzzz"));
    device_list->addItem(new QListWidgetItem(QIcon(":/img/camera.png"),"yyyyyyyyyy"));
    device_list->addItem(new QListWidgetItem(QIcon(":/img/camera.png"),"ccccccccccc"));

    left_layout->addStretch();
}

void MonitorWidget::init_qss()
{
    QFile file(":/qss/monitor.qss");
    if(file.open(QFile::ReadOnly)){
        this->setStyleSheet(file.readAll());
        file.close();
    }
}

void MonitorWidget::init_rightwidget()
{
    //上方是视频监控区，下方是操作按钮，设为垂直布局
    QVBoxLayout* right_layout=new QVBoxLayout;
    right_widget->setLayout(right_layout);

    //设置上方监控视频区为堆叠布局
    video_layout=new QStackedLayout;

    //创捷单通道窗口
    QWidget *w1=new QWidget;
    video_layout->addWidget(w1);

    //创建四通道
    QWidget *w4=new QWidget;
    video_layout->addWidget(w4);
    right_layout->addLayout(video_layout,8);

    //初始化单通道窗口
    QVBoxLayout *w1_layout=new QVBoxLayout;
    w1->setLayout(w1_layout);
    player = new QtVideoPlayer(w1);
    w1_layout->addWidget(player->videoWidget());
    playStatus = new QLabel("选择本地视频开始播放");
    right_layout->addWidget(playStatus);
    QHBoxLayout *playButtons = new QHBoxLayout;
    QPushButton *open = new QPushButton("打开视频");
    QPushButton *pause = new QPushButton("暂停");
    QPushButton *resume = new QPushButton("继续");
    QPushButton *stop = new QPushButton("停止");
    playButtons->addWidget(open);
    playButtons->addWidget(pause);
    playButtons->addWidget(resume);
    playButtons->addWidget(stop);
    right_layout->addLayout(playButtons);
    connect(open, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getOpenFileName(this, "选择本地视频", QString(), "视频 (*.mp4 *.avi *.mkv *.wmv);;所有文件 (*)");
        if (!path.isEmpty()) {
            switch_to_channel1();
            player->open(path);
        }
    });
    connect(pause, &QPushButton::clicked, player, &VideoPlayer::pause);
    connect(resume, &QPushButton::clicked, player, &VideoPlayer::resume);
    connect(stop, &QPushButton::clicked, player, &VideoPlayer::stop);
    connect(player, &VideoPlayer::statusChanged, playStatus, &QLabel::setText);
    connect(player, &VideoPlayer::playbackError, playStatus, &QLabel::setText);
    //初始化四窗口
    int x=0,y=0;
    QGridLayout *w4_layout=new QGridLayout;
    w4->setLayout(w4_layout);
    for (int i=0;i<4;i++) {
        lab_videos[i]=new QLabel;
        lab_videos[i]->setPixmap(QPixmap(":/img/摄像头_关闭.png"));
        lab_videos[i]->setScaledContents(true);//内容随窗口缩放
        lab_videos[i]->setAlignment(Qt::AlignCenter);//设置内容居中
        switch (i) {
            case 0:x=y=0;break;
            case 1:x=0;y=1;break;
            case 2:x=1;y=0;break;
            case 3:x=1;y=1;break;
        }
        w4_layout->addWidget(lab_videos[i],x,y);
    }

    //设置下方按钮区为水平布局
    QHBoxLayout *btn_layout=new QHBoxLayout;
    btn_channel1=new QPushButton(QIcon(":/img/单窗口.png"),"");
    btn_channel4=new QPushButton(QIcon(":/img/多窗口0.png"),"");
    btn_channel1->setIconSize(QSize(80,80));
    btn_channel4->setIconSize(QSize(80,80));
    btn_layout->addWidget(btn_channel1);
    btn_layout->addWidget(btn_channel4);
    btn_layout->addStretch();
    btn_layout->setSpacing(30);
    right_layout->addLayout(btn_layout,1);
}

void MonitorWidget::init_connect()
{
    connect(btn_channel1,SIGNAL(clicked()),this,SLOT(switch_to_channel1()));
    connect(btn_channel4,SIGNAL(clicked()),this,SLOT(switch_to_channel4()));
}

void MonitorWidget::switch_to_channel1()
{
    video_layout->setCurrentIndex(0);
}

void MonitorWidget::switch_to_channel4()
{
    video_layout->setCurrentIndex(1);
}

