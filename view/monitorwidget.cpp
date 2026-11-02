#include "monitorwidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QFile>
#include "../service/apiclient.h"
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QJsonArray>

MonitorWidget::MonitorWidget(QWidget *parent) : QWidget(parent)
{
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    QWidget *left = new QWidget(this);
    left->setObjectName("left_box");
    QVBoxLayout *leftLayout = new QVBoxLayout(left);
    left->setMinimumWidth(180);
    left->setMaximumWidth(240);
    QLabel *listTitle = new QLabel("预览通道（尚未接设备管理）");
    listTitle->setWordWrap(true);
    leftLayout->addWidget(listTitle);
    channelList = new QListWidget(left);
    for (int i=0;i<4;i++) channelList->addItem(QString("预览通道 %1").arg(i+1));
    channelList->setCurrentRow(0);
    leftLayout->addWidget(channelList);
    leftLayout->addWidget(new QLabel("服务端摄像头（双击打开）"));
    deviceList=new QListWidget(left);
    deviceStatus=new QLabel("登录后刷新设备",left);deviceStatus->setWordWrap(true);
    QPushButton *refresh=new QPushButton("刷新设备",left);
    leftLayout->addWidget(deviceList);leftLayout->addWidget(deviceStatus);leftLayout->addWidget(refresh);
    QPushButton *capture=new QPushButton("录制所选设备6秒",left);leftLayout->addWidget(capture);
    connect(capture,&QPushButton::clicked,this,[this](){
        if(!deviceList->currentItem()){deviceStatus->setText("先选择服务端摄像头");return;}
        QString camera=deviceList->currentItem()->data(Qt::UserRole).toString();
        struct Result {QJsonObject data;QString error;};QFutureWatcher<Result> *watcher=new QFutureWatcher<Result>(this);
        connect(watcher,&QFutureWatcher<Result>::finished,this,[this,watcher](){Result result=watcher->result();watcher->deleteLater();deviceStatus->setText(result.error.isEmpty()?"后台录制中，稍后到回看页刷新录像列表。":result.error);});
        watcher->setFuture(QtConcurrent::run([camera](){Result result;result.data=ApiClient::request("/api/recordings/capture",QJsonObject{{"cameraId",camera},{"seconds",6}},true,&result.error);return result;}));
    });
    connect(refresh,&QPushButton::clicked,this,&MonitorWidget::refreshDevices);
    connect(deviceList,&QListWidget::itemDoubleClicked,this,[this](QListWidgetItem *item) {
        QString camera=item->data(Qt::UserRole).toString();int channel=selectedChannel;
        struct Result {QJsonObject data;QString error;};
        QFutureWatcher<Result> *watcher=new QFutureWatcher<Result>(this);
        connect(watcher,&QFutureWatcher<Result>::finished,this,[this,watcher,channel]() {
            Result result=watcher->result();watcher->deleteLater();
            if(!result.error.isEmpty()){deviceStatus->setText(result.error);return;}
            openSource(channel,result.data.value("url").toString());
        });
        watcher->setFuture(QtConcurrent::run([camera](){Result result;result.data=ApiClient::request("/api/media/play",QJsonObject{{"cameraId",camera}},true,&result.error);return result;}));
    });
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
    // 按功能分三行，窗口变小时不再被一长排按钮撑住。
    QGridLayout *buttons = new QGridLayout;
    QPushButton *single = new QPushButton("单画面");
    QPushButton *four = new QPushButton("四画面");
    QPushButton *open = new QPushButton("打开本地视频");
    QPushButton *stream = new QPushButton("打开RTSP");
    QPushButton *pause = new QPushButton("暂停当前");
    QPushButton *resume = new QPushButton("继续/重播当前");
    QPushButton *stop = new QPushButton("停止当前");
    QPushButton *pauseAll = new QPushButton("全部暂停");
    QPushButton *resumeAll = new QPushButton("全部继续/重播");
    QPushButton *stopAllButton = new QPushButton("全部停止");
    pauseAll->setObjectName("pauseAll");
    resumeAll->setObjectName("resumeAll");
    stopAllButton->setObjectName("stopAll");
    buttons->addWidget(single,0,0); buttons->addWidget(four,0,1);
    buttons->addWidget(open,0,2); buttons->addWidget(stream,0,3);
    buttons->addWidget(pause,1,0); buttons->addWidget(resume,1,1);
    buttons->addWidget(stop,1,2);
    buttons->addWidget(pauseAll,2,0); buttons->addWidget(resumeAll,2,1);
    buttons->addWidget(stopAllButton,2,2);
    rightLayout->addLayout(buttons);
    QLabel *hint = new QLabel("左侧选择通道后打开视频。暂停后继续；停止或结束后从头重播。预览静音。");
    hint->setWordWrap(true);
    rightLayout->addWidget(hint);
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
    connect(pauseAll,&QPushButton::clicked,this,[this](){
        for(int i=0;i<4;i++) players[i]->pause();
    });
    connect(resumeAll,&QPushButton::clicked,this,[this](){
        for(int i=0;i<4;i++) players[i]->resume();
    });
    connect(stopAllButton,&QPushButton::clicked,this,&MonitorWidget::stopAll);
    QFile qss(":/qss/monitor.qss");
    if(qss.open(QIODevice::ReadOnly)) setStyleSheet(qss.readAll());
    updateLayout();
}
void MonitorWidget::openSource(int channel,const QString &source)
{
    if(channel<0 || channel>=4) return;
    channelList->setCurrentRow(channel); // 回放指定通道时同步选择，避免视频在隐藏槽播放。
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

void MonitorWidget::refreshDevices()
{
    deviceStatus->setText("正在读取服务端设备……");
    struct Result {QJsonObject data;QString error;};
    QFutureWatcher<Result> *watcher=new QFutureWatcher<Result>(this);
    connect(watcher,&QFutureWatcher<Result>::finished,this,[this,watcher]() {
        Result result=watcher->result();watcher->deleteLater();deviceList->clear();
        if(!result.error.isEmpty()){deviceStatus->setText(result.error);return;}
        QJsonArray devices=result.data.value("cameras").toArray();
        for(int i=0;i<devices.size();i++) {
            QJsonObject camera=devices[i].toObject();
            QListWidgetItem *item=new QListWidgetItem(camera.value("name").toString()+" ["+camera.value("status").toString()+"]",deviceList);
            item->setData(Qt::UserRole,camera.value("id").toString());
        }
        deviceStatus->setText(QString("可访问设备 %1 个").arg(devices.size()));
    });
    watcher->setFuture(QtConcurrent::run([](){Result result;result.data=ApiClient::request("/api/cameras",{},false,&result.error);return result;}));
}
