#include "loginwidget.h"
#include "../service/apiclient.h"
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QCloseEvent>
#include <QCheckBox>
#include "../util/winmanager.h"


LoginWidget::LoginWidget(QWidget *parent) : QWidget(parent)
{
    this->setWindowTitle("登录窗口");
    this->resize(800,600);
    QPixmap pm(":/img/e33a1b4319df49871b92f8b8cd82c6f2.png");
    QPalette pal=this->palette();
    pal.setBrush(QPalette::Window,QBrush(pm.scaled(this->width(),this->height())));
    this->setAutoFillBackground(true);
    this->setPalette(pal);



    QVBoxLayout *main_layout=new QVBoxLayout;
    this->setLayout(main_layout);

    QLabel *lab_title=new QLabel("登录");
    lab_title->setFont(QFont("宋体",20,QFont::Bold));
    lab_title->setAlignment(Qt::AlignCenter);//设置对齐方式为水平+垂直居中



    //表单布局
    QHBoxLayout *h_layout=new QHBoxLayout;
    QFormLayout *form_layout=new QFormLayout;
    edit_user=new QLineEdit;
    edit_user->setObjectName("login_username");
    edit_user->setPlaceholderText("admin 或 user");
    ediy_pwd=new QLineEdit;
    ediy_pwd->setObjectName("login_password");
    ediy_pwd->setEchoMode(QLineEdit::Password);
    form_layout->addRow("用户名：",edit_user);
    form_layout->addRow("密码",ediy_pwd);
    QCheckBox *showPassword=new QCheckBox("显示密码");
    connect(showPassword,&QCheckBox::toggled,this,[this](bool visible) {
        ediy_pwd->setEchoMode(visible ? QLineEdit::Normal : QLineEdit::Password);
    });
    form_layout->addRow(showPassword);

    //验证码布局容器
    QHBoxLayout *vcode_layout=new QHBoxLayout;
    QLabel *lay_vcode=new QLabel("验证码");
    edit_vcode=new QLineEdit;
    edit_vcode->setObjectName("edit_vcode");   //给 qss 单独定字号用
    edit_vcode->setFixedWidth(80);
    edit_vcode->setMaxLength(4);
    edit_vcode->setAlignment(Qt::AlignCenter);
    vcode_label=new VerifyCodeLabel;           //验证码图片，点一下换一张
    vcode_layout->addWidget(lay_vcode);
    vcode_layout->addWidget(edit_vcode);
    vcode_layout->addWidget(vcode_label);
    form_layout->addRow(vcode_layout);

    //按钮容器布局
    QHBoxLayout* btn_layout=new QHBoxLayout;
    btn_login=new QPushButton("登录");
    QPushButton* btn_cancel=new QPushButton("返回主界面");
    btn_cancel->setObjectName("login_back");
    // close()统一走closeEvent，按钮和右上角关闭使用相同的返回逻辑。
    connect(btn_cancel,&QPushButton::clicked,this,&QWidget::close);
    btn_layout->addWidget(btn_login);
    btn_layout->addWidget(btn_cancel);
    form_layout->addRow(btn_layout);

    form_layout->setSpacing(30);
    h_layout->addStretch();
    h_layout->addLayout(form_layout);
    h_layout->addStretch();

    main_layout->addStretch(2);
    main_layout->addWidget(lab_title);
    main_layout->addStretch(1);
    main_layout->addLayout(h_layout);
    main_layout->addStretch(2);

    this->init_qss();
    this->init_connect();
}

void LoginWidget::init_qss()
{
    QFile file(":/qss/login_reg.qss");
    if(file.open(QFile::ReadOnly)){
        this->setStyleSheet(file.readAll());
        file.close();
    }
}

void LoginWidget::init_connect()
{
    connect(btn_login,SIGNAL(clicked()),this,SLOT(login()));

    //点了验证码图片，图换了，输入框里旧的也一并清掉
    connect(vcode_label,&VerifyCodeLabel::clicked,edit_vcode,&QLineEdit::clear);
}

void LoginWidget::login()
{
    if(!btn_login->isEnabled())return;
    QString username=edit_user->text().trimmed();
    QString password=ediy_pwd->text();
    QString vcode=edit_vcode->text().trimmed();
    if(username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this,"登录提示","请输入用户名和密码");
        return;
    }

    //先判断验证码，不通过就不去查数据库
    if(vcode.isEmpty()||vcode.compare(vcode_label->code(),Qt::CaseInsensitive)!=0){
        QMessageBox::warning(this,"登录警告","验证码错误，请重新输入");
        vcode_label->refresh();
        edit_vcode->clear();
        edit_vcode->setFocus();
        return;
    }

    // HTTP登录放到工作线程，避免等待网络时卡住登录窗口。
    struct LoginResult {QJsonObject data;QString error;};
    btn_login->setEnabled(false);
    int attempt=++loginAttempt;
    ediy_pwd->clear();
    QFutureWatcher<LoginResult> *watcher=new QFutureWatcher<LoginResult>(this);
    connect(watcher,&QFutureWatcher<LoginResult>::finished,this,[this,watcher,username,attempt]() {
        LoginResult result=watcher->result();watcher->deleteLater();
        // 用户已返回时，不弹旧错误，也不让旧请求突然把账号登录进去。
        if(attempt!=loginAttempt)return;
        btn_login->setEnabled(true);
        if(!result.error.isEmpty() || result.data.value("token").toString().isEmpty()) {
            QMessageBox::warning(this,"登录失败",result.error.isEmpty()?"服务端响应无令牌":result.error);
            vcode_label->refresh();edit_vcode->clear();return;
        }
        ApiSession::setToken(result.data.value("token").toString());
        WinManager::getInstence()->mainWidget.showUsername(username);
        hide();
    });
    watcher->setFuture(QtConcurrent::run([username,password]() {
        LoginResult result;
        result.data=ApiClient::request("/api/login",QJsonObject{{"username",username},{"password",password}},true,&result.error);
        return result;
    }));
}

void LoginWidget::closeEvent(QCloseEvent *event)
{
    ++loginAttempt;
    btn_login->setEnabled(true);
    ediy_pwd->clear();
    edit_vcode->clear();
    vcode_label->refresh();
    // 返回只切换窗口，不会授予登录权限，原有会话也不会在此修改。
    WinManager::getInstence()->mainWidget.show();
    event->accept();
}
