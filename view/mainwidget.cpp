#include "mainwidget.h"
#include "../util/winmanager.h"
#include <QGuiApplication>
#include <QScreen>

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
    connect(r_widget,&ReviewWidget::recordingReady,this,[this,m_widget](const QString &path){
        buttom_layout->setCurrentIndex(0);m_widget->switch_to_channel1();m_widget->openSource(0,path);
    });
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
    QPushButton *btn_setting=new QPushButton(QIcon(":/img/设置.png"),"设置");
    this->top_layout->addWidget(btn_setting);

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
