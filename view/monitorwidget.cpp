#include "monitorwidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QFile>

MonitorWidget::MonitorWidget(QWidget *parent) : QWidget(parent)
{
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    QWidget *left = new QWidget(this);
    left->setObjectName("left_box");
    QVBoxLayout *leftLayout = new QVBoxLayout(left);
    left->setMinimumWidth(260);
    QLabel *listTitle = new QLabel("预览通道（尚未接设备管理）");
    listTitle->setWordWrap(true);
    leftLayout->addWidget(listTitle);
    channelList = new QListWidget(left);
    for (int i=0;i<4;i++) channelList->addItem(QString("预览通道 %1").arg(i+1));
    channelList->setCurrentRow(0);
    leftLayout->addWidget(channelList);
    mainLayout->addWidget(left,1);

    QWidget *right = new QWidget(this);
    QVBoxLayout *rightLayout = new QVBoxLayout(right);
    grid = new QGridLayout;
    rightLayout->addLayout(grid,1);
    mainLayout->addWidget(right,6);
    // 四个通道各有自己的线程、进程和容量1帧缓存，不共用停止标志。
    for (int i=0;i<4;i++) {
        tiles[i] = new QWidget(right);
        QVBoxLayout *tileLayout = new QVBoxLayout(tiles[i]);
        tileLayout->addWidget(new QLabel(QString("通道 %1").arg(i+1)));
        players[i] = new FfmpegPlayer(tiles[i]);
        players[i]->setObjectName(QString("previewPlayer%1").arg(i));
        tileLayout->addWidget(players[i]->videoWidget(),1);
        QLabel *status = new QLabel("未打开视频",tiles[i]);
        status->setWordWrap(true);
        tileLayout->addWidget(status);
        connect(players[i],&VideoPlayer::statusChanged,status,&QLabel::setText);
        connect(players[i],&VideoPlayer::playbackError,status,&QLabel::setText);
    }
    QHBoxLayout *buttons = new QHBoxLayout;
    QPushButton *single = new QPushButton("单画面");
    QPushButton *four = new QPushButton("四画面");
    QPushButton *open = new QPushButton("打开本地视频");
    QPushButton *stream = new QPushButton("打开RTSP");
    QPushButton *pause = new QPushButton("暂停当前");
    QPushButton *resume = new QPushButton("继续当前");
    QPushButton *stop = new QPushButton("停止当前");
    QPushButton *stopAllButton = new QPushButton("全部停止");
    buttons->addWidget(single); buttons->addWidget(four);
    buttons->addWidget(open); buttons->addWidget(stream);
    buttons->addWidget(pause); buttons->addWidget(resume);
    buttons->addWidget(stop); buttons->addWidget(stopAllButton);
    rightLayout->addLayout(buttons);
    rightLayout->addWidget(new QLabel("先在左侧选择通道再打开视频。预览为静音；切换布局保留各通道播放。"));
    connect(single,&QPushButton::clicked,this,&MonitorWidget::switch_to_channel1);
    connect(four,&QPushButton::clicked,this,&MonitorWidget::switch_to_channel4);
    connect(channelList,&QListWidget::currentRowChanged,this,[this](int row){
        if(row>=0 && row<4) {selectedChannel=row;updateLayout();}
    });
    connect(open,&QPushButton::clicked,this,[this](){
        QString path=QFileDialog::getOpenFileName(this,"打开当前通道视频",QString(),"视频 (*.mp4 *.avi *.mkv *.wmv);;全部文件 (*)");
        if(!path.isEmpty()) openSource(selectedChannel,path);
    });
    connect(stream,&QPushButton::clicked,this,[this](){
        bool accepted=false;
        QString url=QInputDialog::getText(this,"打开当前通道RTSP","地址仅用于本次预览",QLineEdit::Normal,
                                         "rtsp://127.0.0.1:18554/demo",&accepted);
        if(accepted && !url.trimmed().isEmpty()) openSource(selectedChannel,url.trimmed());
    });
    connect(pause,&QPushButton::clicked,this,[this](){players[selectedChannel]->pause();});
    connect(resume,&QPushButton::clicked,this,[this](){players[selectedChannel]->resume();});
    connect(stop,&QPushButton::clicked,this,[this](){players[selectedChannel]->stop();});
    connect(stopAllButton,&QPushButton::clicked,this,&MonitorWidget::stopAll);
    QFile qss(":/qss/monitor.qss");
    if(qss.open(QIODevice::ReadOnly)) setStyleSheet(qss.readAll());
    updateLayout();
}
void MonitorWidget::openSource(int channel,const QString &source)
{
    if(channel<0 || channel>=4) return;
    players[channel]->open(source);
}
void MonitorWidget::stopAll() { for(int i=0;i<4;i++) players[i]->stop(); }
MonitorWidget::~MonitorWidget() { stopAll(); }
void MonitorWidget::switch_to_channel1() { fourViews=false;updateLayout(); }
void MonitorWidget::switch_to_channel4() { fourViews=true;updateLayout(); }
void MonitorWidget::updateLayout()
{
    // 复用同一组显示控件，切布局不新建播放器，也不重复解码。
    for(int i=0;i<4;i++) {
        grid->removeWidget(tiles[i]);
        bool visible = fourViews || i==selectedChannel;
        tiles[i]->setVisible(visible);
        if(visible) {
            if(fourViews) grid->addWidget(tiles[i],i/2,i%2);
            else grid->addWidget(tiles[i],0,0);
        }
    }
    grid->setRowStretch(0,1);grid->setColumnStretch(0,1);
    grid->setRowStretch(1,fourViews?1:0);grid->setColumnStretch(1,fourViews?1:0);
}
