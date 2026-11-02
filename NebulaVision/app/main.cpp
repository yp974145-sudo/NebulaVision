#include <QApplication>
#include <QtWidgets>
#include <QtSql>
#include <QMediaPlayer>
#include <QVideoWidget>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QAudioOutput>
#endif
int main(int argc,char**argv){
 QApplication app(argc,argv);app.setApplicationName("NebulaVision");app.setApplicationVersion("1.0.0");
 QMainWindow win;win.setWindowTitle("NebulaVision 星云智视 1.0 · 基础验证版");win.resize(1100,760);
 auto tabs=new QTabWidget;win.setCentralWidget(tabs);
 auto home=new QWidget;auto hl=new QVBoxLayout(home);
 auto title=new QLabel("<h1>NebulaVision 星云智视</h1><p>1.0 基础验证版 · 独立开发库</p>");hl->addWidget(title);
 auto status=new QLabel("数据库未连接；本地视频播放可独立使用");status->setWordWrap(true);hl->addWidget(status);
 auto user=new QLineEdit("nebulavision_app");auto password=new QLineEdit;password->setEchoMode(QLineEdit::Password);
 auto form=new QFormLayout;form->addRow("新库账号",user);form->addRow("密码（仅本次进程使用）",password);hl->addLayout(form);
 auto connectBtn=new QPushButton("连接 nebulavision_dev");hl->addWidget(connectBtn);
 auto info=new QLabel("本版：新库只读通道与事件查询、本地视频播放。\n演示数据为合成数据；录像、实时摄像头、AI检测、Agent和用户登录尚未实现。\n密码不保存，不访问原 smart_monitor 数据库。");info->setWordWrap(true);hl->addWidget(info);hl->addStretch();tabs->addTab(home,"连接与说明");
 auto cameras=new QTableWidget;auto events=new QTableWidget;
 auto makePage=[&](QTableWidget*table,QString text){auto page=new QWidget;auto l=new QVBoxLayout(page);l->addWidget(new QLabel(text));auto refresh=new QPushButton("刷新");l->addWidget(refresh);table->setEditTriggers(QAbstractItemView::NoEditTriggers);table->setSelectionBehavior(QAbstractItemView::SelectRows);table->horizontalHeader()->setStretchLastSection(true);l->addWidget(table);return qMakePair(page,refresh);};
 auto cp=makePage(cameras,"通道列表 · 本版仅展示，不修改设备");auto ep=makePage(events,"最近100条事件 · 合成测试事件不代表实际检测结果");tabs->addTab(cp.first,"通道");tabs->addTab(ep.first,"事件");
 auto videoPage=new QWidget;auto vl=new QVBoxLayout(videoPage);auto video=new QVideoWidget;auto player=new QMediaPlayer(&win);player->setVideoOutput(video);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
 auto audio=new QAudioOutput(player);player->setAudioOutput(audio);
#endif
 vl->addWidget(video,1);auto videoStatus=new QLabel("选择本地视频；支持格式取决于本机媒体后端");vl->addWidget(videoStatus);auto buttons=new QHBoxLayout;auto open=new QPushButton("打开视频");auto pause=new QPushButton("播放 / 暂停");auto stop=new QPushButton("停止");buttons->addWidget(open);buttons->addWidget(pause);buttons->addWidget(stop);vl->addLayout(buttons);tabs->addTab(videoPage,"本地播放");
 auto db=QSqlDatabase::addDatabase("QMYSQL","nebulavision_v1");db.setHostName("127.0.0.1");db.setPort(3306);db.setDatabaseName("nebulavision_dev");db.setConnectOptions("MYSQL_OPT_CONNECT_TIMEOUT=3");
 auto load=[&](QTableWidget*t,QString sql,QStringList headers){t->clear();t->setRowCount(0);t->setColumnCount(headers.size());t->setHorizontalHeaderLabels(headers);if(!db.isOpen()){status->setText("请先连接新开发库");return;}QSqlQuery q(db);if(!q.exec(sql)){status->setText("查询失败："+q.lastError().text());return;}while(q.next()){int row=t->rowCount();t->insertRow(row);for(int c=0;c<headers.size();++c)t->setItem(row,c,new QTableWidgetItem(q.value(c).toString()));}t->resizeColumnsToContents();};
 auto loadCameras=[&]{load(cameras,"SELECT id,name,status,enabled FROM cameras ORDER BY id LIMIT 100",{"编号","通道名称","状态","启用"});};
 auto loadEvents=[&]{load(events,"SELECT e.id,c.name,e.occurred_at,e.event_type,e.description,e.status FROM events e JOIN cameras c ON c.id=e.camera_id ORDER BY e.occurred_at DESC,e.id DESC LIMIT 100",{"编号","通道","时间 UTC","类型","描述","状态"});};
 QObject::connect(cp.second,&QPushButton::clicked,loadCameras);QObject::connect(ep.second,&QPushButton::clicked,loadEvents);
 QObject::connect(connectBtn,&QPushButton::clicked,[&]{if(user->text().trimmed().isEmpty()){status->setText("请输入新库账号");return;}db.close();db.setUserName(user->text().trimmed());db.setPassword(password->text());if(!db.open()){status->setText("连接失败："+db.lastError().text());return;}QSqlQuery q(db);if(!q.exec("SELECT DATABASE()")||!q.next()||q.value(0).toString()!="nebulavision_dev"){db.close();status->setText("已拒绝连接：数据库名称校验失败");return;}status->setText("已连接 nebulavision_dev · 业务操作仅执行只读查询");password->clear();loadCameras();loadEvents();});
 QObject::connect(open,&QPushButton::clicked,[&]{auto path=QFileDialog::getOpenFileName(&win,"打开视频",QString(),"视频 (*.mp4 *.avi *.mkv *.mov);;全部文件 (*)");if(!path.isEmpty()){videoStatus->setText(QFileInfo(path).fileName());
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
player->setSource(QUrl::fromLocalFile(path));
#else
player->setMedia(QUrl::fromLocalFile(path));
#endif
player->play();}});
 QObject::connect(pause,&QPushButton::clicked,[&]{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
if(player->playbackState()==QMediaPlayer::PlayingState)
#else
if(player->state()==QMediaPlayer::PlayingState)
#endif
player->pause();else player->play();});QObject::connect(stop,&QPushButton::clicked,player,&QMediaPlayer::stop);

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
 QObject::connect(player,&QMediaPlayer::errorOccurred,[&](QMediaPlayer::Error,const QString &message){videoStatus->setText("播放失败："+message);});
#else
 QObject::connect(player,QOverload<QMediaPlayer::Error>::of(&QMediaPlayer::error),[&](QMediaPlayer::Error){videoStatus->setText("播放失败："+player->errorString());});
#endif
 app.setStyleSheet("QWidget{font-size:14px;} QMainWindow{background:#f4f6fa;} QPushButton{padding:9px;} QTabBar::tab{padding:12px;} QTableWidget{background:white;}");
 QFile localConfig(QCoreApplication::applicationDirPath()+"/../config/viewer.local.json");if(localConfig.open(QIODevice::ReadOnly)){auto config=QJsonDocument::fromJson(localConfig.readAll()).object();if(config.value("database").toString()=="nebulavision_dev"){user->setText(config.value("username").toString());password->setText(config.value("password").toString());connectBtn->click();}} win.show();int result=app.exec();player->stop();db.close();return result;
}

