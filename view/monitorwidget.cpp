#include "monitorwidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QInputDialog>
#include <QFile>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QJsonArray>
#include <QSysInfo>
#include <QCryptographicHash>
#include <QDialog>
#include <QFormLayout>
#include <QSpinBox>
#include <QLineEdit>
#include <QUrl>
#include "../service/apiclient.h"
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
#include <QMediaDevices>
#endif
namespace {
QString localKey(const QByteArray &id){return QString::fromLatin1(QCryptographicHash::hash(QSysInfo::machineUniqueId()+id,QCryptographicHash::Sha256).toHex());}
struct ApiResult{QJsonObject data;QString error;};
}
MonitorWidget::MonitorWidget(QWidget *parent):QWidget(parent){
    auto main=new QHBoxLayout(this);auto left=new QWidget(this);left->setObjectName("left_box");left->setMinimumWidth(205);left->setMaximumWidth(260);auto side=new QVBoxLayout(left);
    side->addWidget(new QLabel("画面位置"));channelList=new QComboBox;channelList->setObjectName("channelSelector");for(int i=0;i<4;i++)channelList->addItem(QString("通道 %1").arg(i+1));side->addWidget(channelList);
    side->addWidget(new QLabel("设备列表（双击打开）"));deviceList=new QListWidget;deviceList->setObjectName("cameraDevices");deviceList->setTextElideMode(Qt::ElideRight);side->addWidget(deviceList,1);
    deviceStatus=new QLabel("请先登录，再刷新设备");deviceStatus->setWordWrap(true);side->addWidget(deviceStatus);
    auto refresh=new QPushButton("刷新设备");auto add=new QPushButton("添加海康/网络设备");auto capture=new QPushButton("录制所选网络设备6秒");side->addWidget(refresh);side->addWidget(add);side->addWidget(capture);main->addWidget(left);
    auto right=new QWidget;auto layout=new QVBoxLayout(right);grid=new QGridLayout;layout->addLayout(grid,1);main->addWidget(right,1);
    for(int i=0;i<4;i++){
        tiles[i]=new QWidget(right);auto tile=new QVBoxLayout(tiles[i]);tile->setContentsMargins(3,3,3,3);tile->addWidget(new QLabel(QString("通道 %1").arg(i+1)));
        players[i]=new FfmpegPlayer(tiles[i]);players[i]->setObjectName(QString("previewPlayer%1").arg(i));tile->addWidget(players[i]->videoWidget(),1);
    }
    auto buttons=new QHBoxLayout;auto single=new QPushButton("单画面");auto four=new QPushButton("四画面");auto open=new QPushButton("打开视频文件");auto stop=new QPushButton("停止当前");auto all=new QPushButton("全部停止");all->setObjectName("stopAll");
    for(auto button:{single,four,open,stop,all})buttons->addWidget(button);layout->addLayout(buttons);
    auto playback=new QHBoxLayout;auto pause=new QPushButton("暂停当前");auto resume=new QPushButton("继续当前");auto pauseAll=new QPushButton("全部暂停");auto resumeAll=new QPushButton("全部继续");pauseAll->setObjectName("pauseAll");resumeAll->setObjectName("resumeAll");
    for(auto button:{pause,resume,pauseAll,resumeAll})playback->addWidget(button);layout->addLayout(playback);
    auto pauseChannel=[this](int i,bool paused){
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
        if(liveViews[i]){if(paused)liveViews[i]->pause();else liveViews[i]->resume();return;}
#endif
        if(paused)players[i]->pause();else players[i]->resume();
    };
    connect(pause,&QPushButton::clicked,this,[=](){pauseChannel(selectedChannel,true);});connect(resume,&QPushButton::clicked,this,[=](){pauseChannel(selectedChannel,false);});
    connect(pauseAll,&QPushButton::clicked,this,[=](){for(int i=0;i<4;i++)pauseChannel(i,true);});connect(resumeAll,&QPushButton::clicked,this,[=](){for(int i=0;i<4;i++)pauseChannel(i,false);});
    connect(single,&QPushButton::clicked,this,&MonitorWidget::switch_to_channel1);connect(four,&QPushButton::clicked,this,&MonitorWidget::switch_to_channel4);
    connect(channelList,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int row){selectedChannel=row;updateLayout();});
    connect(stop,&QPushButton::clicked,this,[this](){stopChannel(selectedChannel);});connect(all,&QPushButton::clicked,this,&MonitorWidget::stopAll);
    connect(open,&QPushButton::clicked,this,[this](){QString path=QFileDialog::getOpenFileName(this,"打开视频",{},"视频 (*.mp4 *.avi *.mkv)");if(!path.isEmpty())openSource(selectedChannel,path);});
    connect(refresh,&QPushButton::clicked,this,&MonitorWidget::refreshDevices);
    connect(deviceList,&QListWidget::itemDoubleClicked,this,[this](QListWidgetItem *item){openDevice(item->data(Qt::UserRole).toJsonObject());});
    connect(capture,&QPushButton::clicked,this,[this](){
        if(!deviceList->currentItem()){deviceStatus->setText("先选择网络设备");return;}auto device=deviceList->currentItem()->data(Qt::UserRole).toJsonObject();
        if(device.value("sourceType").toString()=="local"){deviceStatus->setText("本机设备已接实时检测与事件入库；此按钮用于后台网络录像。");return;}
        auto watcher=new QFutureWatcher<ApiResult>(this);connect(watcher,&QFutureWatcher<ApiResult>::finished,this,[this,watcher](){auto r=watcher->result();watcher->deleteLater();deviceStatus->setText(r.error.isEmpty()?"后台录制中，稍后到回放页刷新录像。":r.error);});
        watcher->setFuture(QtConcurrent::run([device](){ApiResult r;r.data=ApiClient::request("/api/recordings/capture",{{"cameraId",device.value("id")},{"seconds",6}},true,&r.error);return r;}));
    });
    connect(add,&QPushButton::clicked,this,[this](){
        if(ApiSession::token().isEmpty()){deviceStatus->setText("请先以管理员登录");return;}
        auto dialog=new QDialog(this);dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->setWindowTitle("添加海康/网络设备");auto form=new QFormLayout(dialog);
        auto name=new QLineEdit("海康摄像头");auto host=new QLineEdit;host->setPlaceholderText("摄像头IP");auto user=new QLineEdit("admin");auto password=new QLineEdit;password->setEchoMode(QLineEdit::Password);
        auto port=new QSpinBox;port->setRange(1,65535);port->setValue(554);auto channel=new QSpinBox;channel->setRange(1,256);auto stream=new QComboBox;stream->addItems({"子码流（低带宽）","主码流"});
        form->addRow("设备名称",name);form->addRow("IP/主机名",host);form->addRow("RTSP端口",port);form->addRow("摄像头账号",user);form->addRow("摄像头密码",password);form->addRow("通道",channel);form->addRow("海康码流",stream);
        auto custom=new QLineEdit;custom->setPlaceholderText("其他型号可填完整RTSP地址（可留空）");custom->setEchoMode(QLineEdit::PasswordEchoOnEdit);form->addRow("自定义RTSP",custom);
        auto result=new QLabel;result->setWordWrap(true);auto save=new QPushButton("添加设备");form->addRow(result);form->addRow(save);
        connect(save,&QPushButton::clicked,dialog,[=](){QUrl url;url.setScheme("rtsp");url.setHost(host->text().trimmed());url.setPort(port->value());url.setUserName(user->text());url.setPassword(password->text());url.setPath(QString("/Streaming/Channels/%1").arg(channel->value()*100+(stream->currentIndex()==0?2:1)));
            QString address=custom->text().trimmed().isEmpty()?url.toString(QUrl::FullyEncoded):custom->text().trimmed();if(!QUrl(address).isValid() || QUrl(address).host().isEmpty()){result->setText("请填写IP或完整RTSP地址");return;}
            if(QUrl(address).scheme()!="rtsp" && QUrl(address).scheme()!="rtsps"){result->setText("需要RTSP视频地址，Seetong等App分享链接不能直接填写。");return;}
            QJsonObject body{{"name",name->text().trimmed()},{"sourceType","rtsp"},{"url",address}};save->setEnabled(false);auto watcher=new QFutureWatcher<ApiResult>(dialog);
            connect(watcher,&QFutureWatcher<ApiResult>::finished,dialog,[=](){auto r=watcher->result();watcher->deleteLater();save->setEnabled(true);if(!r.error.isEmpty()){result->setText(r.error);return;}dialog->close();refreshDevices();});
            watcher->setFuture(QtConcurrent::run([body](){ApiResult r;r.data=ApiClient::request("/api/cameras/register",body,true,&r.error);return r;}));
        });dialog->show();
    });
    QFile qss(":/qss/monitor.qss");if(qss.open(QIODevice::ReadOnly))setStyleSheet(qss.readAll());updateLayout();
}
void MonitorWidget::stopChannel(int channel){++openGeneration[channel];players[channel]->stop();
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    if(liveViews[channel])liveViews[channel]->stop();
#endif
}
void MonitorWidget::stopAll(){for(int i=0;i<4;i++)stopChannel(i);}
MonitorWidget::~MonitorWidget(){stopAll();}
void MonitorWidget::openSource(int channel,const QString &source){if(channel<0 || channel>=4)return;stopChannel(channel);
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    if(liveViews[channel]){liveViews[channel]->hide();liveViews[channel]->deleteLater();liveViews[channel]=nullptr;}
#endif
    players[channel]->videoWidget()->show();channelList->setCurrentIndex(channel);players[channel]->open(source);
}
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
// 把实时检测控件放到已有通道布局里。父对象是通道，不再创建独立窗口。
void MonitorWidget::embedLive(int channel,LivePreview *view){stopChannel(channel);if(liveViews[channel]){liveViews[channel]->hide();liveViews[channel]->deleteLater();}liveViews[channel]=view;players[channel]->videoWidget()->hide();static_cast<QVBoxLayout *>(tiles[channel]->layout())->addWidget(view,1);view->show();}
#endif
void MonitorWidget::openDevice(const QJsonObject &device){
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    int channel=selectedChannel;int generation=++openGeneration[channel];QString type=device.value("sourceType").toString(),id=device.value("id").toString();
    if(type=="local"){
        for(const auto &camera:QMediaDevices::videoInputs())if(localKey(camera.id())==device.value("sourceKey").toString()){
            auto view=new LivePreview(tiles[channel]);view->setCameraId(id);embedLive(channel,view);view->openCamera(camera);return;
        }
        deviceStatus->setText("此本机设备不在当前电脑上或已拔出");return;
    }
    auto watcher=new QFutureWatcher<ApiResult>(this);connect(watcher,&QFutureWatcher<ApiResult>::finished,this,[this,watcher,channel,id,generation](){auto r=watcher->result();watcher->deleteLater();if(generation!=openGeneration[channel])return;if(!r.error.isEmpty()){deviceStatus->setText(r.error);return;}auto view=new LivePreview(tiles[channel]);view->setCameraId(id);embedLive(channel,view);view->openStream(r.data.value("url").toString());});
    watcher->setFuture(QtConcurrent::run([type,id](){ApiResult r;if(type=="rtsp")r.data=ApiClient::request("/api/cameras/source?id="+id,{},false,&r.error);else r.data=ApiClient::request("/api/media/play",{{"cameraId",id}},true,&r.error);return r;}));
#else
    Q_UNUSED(device);
#endif
}
void MonitorWidget::switch_to_channel1(){fourViews=false;updateLayout();}
void MonitorWidget::switch_to_channel4(){fourViews=true;updateLayout();}
void MonitorWidget::updateLayout(){for(int i=0;i<4;i++){grid->removeWidget(tiles[i]);bool visible=fourViews || i==selectedChannel;tiles[i]->setVisible(visible);if(visible)grid->addWidget(tiles[i],fourViews?i/2:0,fourViews?i%2:0);}grid->setRowStretch(0,1);grid->setColumnStretch(0,1);grid->setRowStretch(1,fourViews?1:0);grid->setColumnStretch(1,fourViews?1:0);}
// 管理员同步当前电脑的摄像头；普通用户只读取后台授权的设备列表。
void MonitorWidget::refreshDevices(){
    if(ApiSession::token().isEmpty()){deviceList->clear();deviceStatus->setText("请先登录");return;}deviceStatus->setText("读取并同步设备……");QJsonArray local;
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    for(const auto &camera:QMediaDevices::videoInputs())local.append(QJsonObject{{"sourceType","local"},{"name",camera.description()},{"sourceKey",localKey(camera.id())}});
#endif
    QString session=ApiSession::token();auto watcher=new QFutureWatcher<ApiResult>(this);connect(watcher,&QFutureWatcher<ApiResult>::finished,this,[this,watcher,session](){auto r=watcher->result();watcher->deleteLater();if(ApiSession::token()!=session)return;deviceList->clear();if(!r.error.isEmpty()){deviceStatus->setText(r.error);return;}
        for(const auto &value:r.data.value("cameras").toArray()){auto camera=value.toObject();QString type=camera.value("sourceType").toString();QString label=(type=="local"?"[本机] ":type=="rtsp"?"[网络] ":"[中继] ")+camera.value("name").toString();auto item=new QListWidgetItem(label,deviceList);item->setToolTip(label);item->setData(Qt::UserRole,camera);}
        deviceStatus->setText("双击设备→当前通道；手势异常自动入库。");
    });watcher->setFuture(QtConcurrent::run([local](){ApiResult r;auto me=ApiClient::request("/api/me",{},false,&r.error);if(!r.error.isEmpty())return r;
        if(me.value("admin").toBool())for(const auto &value:local){ApiClient::request("/api/cameras/register",value.toObject(),true,&r.error);if(!r.error.isEmpty())return r;}
        r.data=ApiClient::request("/api/cameras",{},false,&r.error);return r;}));
}
