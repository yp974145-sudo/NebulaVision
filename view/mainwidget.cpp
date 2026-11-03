#include "mainwidget.h"
#include "../util/winmanager.h"
#include <QGuiApplication>
#include <QScreen>
#include "../service/apiclient.h"
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QDialog>
#include <QPlainTextEdit>
#include <QProcess>
#include <QDir>
#include <QUuid>
#include <QJsonDocument>
#include <QJsonArray>
#include <memory>
#include <QCheckBox>
#include <QInputDialog>
#include <QSaveFile>

MainWidget::MainWidget(QWidget *parent) : QWidget(parent)
{
    this->setWindowTitle("监控系统主页面");
    // 允许适配常见笔记本屏幕，并可拖动窗口边缘缩放。
    this->setMinimumSize(960,640);
    resize(1200,760);
    //窗口主布局采取垂直布局
    QVBoxLayout *main_layout=new QVBoxLayout;
    this->setLayout(main_layout);
    //顶部导航栏布局
    top_layout=new QHBoxLayout;
    //下方窗口（堆叠布局）：第0页监控，第1页回放
    buttom_layout=new QStackedLayout;

    MonitorWidget *m_widget=new MonitorWidget;
    buttom_layout->addWidget(m_widget);

    ReviewWidget *r_widget=new ReviewWidget;
    buttom_layout->addWidget(r_widget);
    // 历史录像由回放页自己的播放器播放，不再覆盖实时监控通道。
    //将两个布局加到主布局
    main_layout->addLayout(top_layout,1);
    main_layout->addLayout(buttom_layout,8);
    //初始化顶部导航窗口
    this->inif_top_widget();
    this->init_qss();
    this->init_connect();
    this->init_showtime();


}

void MainWidget::inif_top_widget()
{
    QLabel *lay_title=new QLabel("安防视频监控管理平台");
    lay_title->setObjectName("title");
    this->top_layout->addWidget(lay_title);
    this->top_layout->addStretch();

    btn_jiankong=new QPushButton(QIcon(":/img/摄像头.png"),"监控");
    this->top_layout->addWidget(btn_jiankong);
    btn_review=new QPushButton(QIcon(":/img/视频回放.png"),"回放");
    this->top_layout->addWidget(btn_review);
    QPushButton *createAccount=new QPushButton("创建账号");top_layout->addWidget(createAccount);
    connect(createAccount,&QPushButton::clicked,this,[this](){
        if(ApiSession::token().isEmpty()){QMessageBox::information(this,"创建账号","请先以管理员身份登录。");return;}
        WinManager::getInstence()->registerWidget.show();hide();
    });
    QPushButton *btn_setting=new QPushButton(QIcon(":/img/设置.png"),"查询助手");
    btn_setting->setObjectName("query_assistant");
    this->top_layout->addWidget(btn_setting);
    connect(btn_setting,&QPushButton::clicked,this,[this]() {
        QDialog *dialog=new QDialog(this);
        dialog->setObjectName("query_assistant_dialog");
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setWindowTitle("业务查询助手");dialog->resize(680,460);
        QVBoxLayout *layout=new QVBoxLayout(dialog);
        layout->addWidget(new QLabel("支持：设备列表、今日事件、YYYY-MM-DD 事件（UTC）、文档 登录失败怎么办"));
        QLineEdit *question=new QLineEdit("设备列表",dialog);
        question->setObjectName("assistant_question");
        QPushButton *run=new QPushButton("查询 / 失败后重试",dialog);
        run->setObjectName("assistant_run");
        QPlainTextEdit *output=new QPlainTextEdit(dialog);output->setReadOnly(true);
        output->setObjectName("assistant_output");
        layout->addWidget(question);layout->addWidget(run);layout->addWidget(output);
        QCheckBox *useDeepSeek=new QCheckBox("使用DeepSeek解读（将问题及查询证据发送到DeepSeek）",dialog);
        useDeepSeek->setObjectName("assistant_deepseek");
        QPushButton *settings=new QPushButton("DeepSeek设置",dialog);
        layout->addWidget(useDeepSeek);layout->addWidget(settings);
        connect(settings,&QPushButton::clicked,dialog,[=]() {
            bool accepted=false;
            QString key=QInputDialog::getText(dialog,"DeepSeek设置",
                "粘贴DeepSeek开放平台API Key，保存在本机，不上传Git：",QLineEdit::Password,QString(),&accepted);
            if(!accepted || key.trimmed().isEmpty())return;
            QString root=QString::fromUtf8(qgetenv("NEBULAVISION_PROJECT_ROOT"));
            if(root.isEmpty())root=QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("..");
            QString resources=QString::fromUtf8(qgetenv("NEBULAVISION_RESOURCES_ROOT"));
            if(resources.isEmpty())resources=QDir(root).absoluteFilePath("../x");
            QSaveFile config(QDir(resources).filePath("deepseek.local.json"));
            QByteArray bytes=QJsonDocument(QJsonObject{{"model","deepseek-flash"},{"api_key",key.trimmed()}}).toJson();
            if(!config.open(QIODevice::WriteOnly) || config.write(bytes)!=bytes.size() || !config.commit()){
                output->setPlainText("DeepSeek设置保存失败，请检查资源目录是否可写。");return;
            }
            config.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner);
            output->setPlainText("DeepSeek密钥已保存。勾选使用DeepSeek后再查询；该选项会发送问题及证据，并产生平台API用量。");
        });
        QProcess *process=new QProcess(dialog);
        QTimer *deadline=new QTimer(dialog);deadline->setSingleShot(true);
        connect(deadline,&QTimer::timeout,process,&QProcess::kill);
        connect(dialog,&QDialog::finished,process,[process]() {if(process->state()!=QProcess::NotRunning)process->kill();});
        // 一个窗口内同一问题使用同一任务编号，失败重试可从checkpoint继续。
        auto task=std::make_shared<QString>(QUuid::createUuid().toString(QUuid::WithoutBraces));
        auto lastQuestion=std::make_shared<QString>();
        connect(run,&QPushButton::clicked,dialog,[=]() {
            if(ApiSession::token().isEmpty()){output->setPlainText("请先登录。");return;}
            if(question->text()!=*lastQuestion){*task=QUuid::createUuid().toString(QUuid::WithoutBraces);*lastQuestion=question->text();}
            QString root=QString::fromUtf8(qgetenv("NEBULAVISION_PROJECT_ROOT"));
            if(root.isEmpty())root=QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("..");
            QString resources=QString::fromUtf8(qgetenv("NEBULAVISION_RESOURCES_ROOT"));
            if(resources.isEmpty())resources=QDir(root).absoluteFilePath("../x");
            QString python=QString::fromUtf8(qgetenv("NEBULAVISION_AGENT_PYTHON"));
            if(python.isEmpty())python=QDir::homePath()+"/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe";
            process->setProgram(python);process->setArguments({root+"/agent/client.py",resources});
            // token通过stdin传递，避免显示在进程命令行中。
            process->setProperty("request",QJsonDocument(QJsonObject{{"token",ApiSession::token()},
                {"question",question->text()},{"taskId",*task},{"resume",true},{"deepseek",useDeepSeek->isChecked()}}).toJson());
            process->setProperty("session",ApiSession::token());
            run->setEnabled(false);output->setPlainText("查询中……");process->start();
        });
        connect(process,&QProcess::started,dialog,[=]() {
            process->write(process->property("request").toByteArray());
            process->setProperty("request",QVariant());process->closeWriteChannel();
            deadline->start(65000);
        });
        connect(process,&QProcess::errorOccurred,dialog,[=](QProcess::ProcessError) {
            deadline->stop();process->setProperty("request",QVariant());
            run->setEnabled(true);output->setPlainText("助手启动或运行失败，请检查Python依赖与本机服务。");
        });
        connect(process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),dialog,[=](int,QProcess::ExitStatus) {
            deadline->stop();
            run->setEnabled(true);
            if(ApiSession::token()!=process->property("session").toString()){
                output->setPlainText("登录状态已改变，请重新查询。");return;
            }
            QJsonObject result=QJsonDocument::fromJson(process->readAllStandardOutput()).object();
            output->setPlainText(result.contains("report")?result.value("report").toString():
                result.value("error").toString("查询失败或超时，可重试。"));
        });
        dialog->show();
    });
    btn_logout=new QPushButton("退出登录");btn_logout->setEnabled(false);top_layout->addWidget(btn_logout);

    lay_time=new QLabel("");
    top_layout->addWidget(lay_time);
    lay_time->setObjectName("ctime");


    QWidget *btn_login_box=new QWidget;
    QVBoxLayout *btn_login_layout=new QVBoxLayout;
    btn_login_box->setLayout(btn_login_layout);
    btn_login=new QPushButton("");
    btn_login->setIcon(QIcon(":/img/未登录-头像.png"));
    btn_login->setIconSize(QSize(30,40));
    lab_login=new QLabel("未登录");
    lab_login->setObjectName("login_lab");
    lab_login->setAlignment(Qt::AlignCenter);
    btn_login_layout->addWidget(btn_login);
    btn_login_layout->addWidget(lab_login);
    btn_login_layout->addStretch();
    top_layout->addWidget(btn_login_box);

}

void MainWidget::init_qss()
{
    QFile file(":/qss/main.qss");
    if(file.open(QFile::ReadOnly)){
        this->setStyleSheet(file.readAll());
        file.close();
    }
}

void MainWidget::init_connect()
{
    connect(btn_login,SIGNAL(clicked()),this,SLOT(to_login_win()));
    connect(btn_logout,&QPushButton::clicked,this,[this](){
        btn_logout->setEnabled(false);btn_login->setEnabled(false);
        QString oldToken=ApiSession::token();QFutureWatcher<QString> *watcher=new QFutureWatcher<QString>(this);
        connect(watcher,&QFutureWatcher<QString>::finished,this,[this,watcher,oldToken](){
            watcher->deleteLater();btn_login->setEnabled(true);
            if(ApiSession::token()==oldToken){ApiSession::setToken(QString());lab_login->setText("未登录");MonitorWidget *monitor=findChild<MonitorWidget *>();if(monitor){monitor->stopAll();monitor->refreshDevices();}auto review=findChild<ReviewWidget *>();if(review)review->clearPlayback();}
        });
        watcher->setFuture(QtConcurrent::run([](){QString error;ApiClient::request("/api/logout",{},true,&error);return error;}));
    });
    //顶部导航切换：监控页/回放页，和单/四通道切换同理，堆叠布局切页
    connect(btn_jiankong,SIGNAL(clicked()),this,SLOT(switch_to_monitor()));
    connect(btn_review,&QPushButton::clicked,this,[]{ WinManager::getInstence()->showReview(); });
}

void MainWidget::switch_to_monitor()
{
    buttom_layout->setCurrentIndex(0);
}

void MainWidget::switch_to_review()
{
    buttom_layout->setCurrentIndex(1);
    static_cast<ReviewWidget *>(buttom_layout->widget(1))->refresh();
}

void MainWidget::init_showtime()
{
    //初始化定时器
    QTimer *m_timer=new QTimer(this);
    //设置定时器时间间隔为1秒
    m_timer->setInterval(1000);

    //连接定时器超时信号到更新时间槽函数
    connect(m_timer,SIGNAL(timeout()),this,SLOT(update_time()));

    //启动定时器
    m_timer->start();

    update_time();
}



void MainWidget::to_login_win()
{
    WinManager::getInstence()->loginWidget.show();
    this->hide();
}

void MainWidget::showUsername(const QString &username)
{
    lab_login->setText(username);
    btn_logout->setEnabled(true);
    MonitorWidget *monitor=findChild<MonitorWidget *>();
    if(monitor)monitor->refreshDevices();
    this->show();
}

void MainWidget::update_time()
{
    //获取当前时间，并进行格式化
    QDateTime currenTime=QDateTime::currentDateTime();
    QString timeString=currenTime.toString("yyyy-MM-dd HH:mm:ss");

    lay_time->setText(timeString);
}
