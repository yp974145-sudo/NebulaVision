#include "loginwidget.h"
#include "../service/userservice.h"
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

    QLabel *lab_title=new QLabel("XX系统登录窗口");
    lab_title->setFont(QFont("宋体",20,QFont::Bold));
    lab_title->setAlignment(Qt::AlignCenter);//设置对齐方式为水平+垂直居中



    //表单布局
    QHBoxLayout *h_layout=new QHBoxLayout;
    QFormLayout *form_layout=new QFormLayout;
    edit_user=new QLineEdit;
    ediy_pwd=new QLineEdit;
    form_layout->addRow("用户名：",edit_user);
    form_layout->addRow("密码",ediy_pwd);

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
    QPushButton* btn_reg=new QPushButton("注册");
    connect(btn_reg,&QPushButton::clicked,this,[this]{
        WinManager::getInstence()->registerWidget.show();
        this->hide();
    });
    QPushButton* btn_cancel=new QPushButton("取消");
    btn_layout->addWidget(btn_login);
    btn_layout->addWidget(btn_reg);
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
    QString username=edit_user->text().trimmed();
    QString password=ediy_pwd->text();
    QString vcode=edit_vcode->text().trimmed();

    //先判断验证码，不通过就不去查数据库
    if(vcode.isEmpty()||vcode.compare(vcode_label->code(),Qt::CaseInsensitive)!=0){
        QMessageBox::warning(this,"登录警告","验证码错误，请重新输入");
        vcode_label->refresh();
        edit_vcode->clear();
        edit_vcode->setFocus();
        return;
    }

    // 验证码通过后，再让业务层验证账号密码，窗口里不直接查数据库。
    UserService userService;
    bool result = userService.login(username, password);
    if (result) {
        // 通过窗口管理器找到原来的主窗口，显示用户名后收起登录窗口。
        WinManager::getInstence()->mainWidget.showUsername(username);
        this->hide();
    } else {
        QMessageBox::warning(this,"登录警告",userService.lastError());
        ediy_pwd->clear();
        edit_vcode->clear();
        vcode_label->refresh();
        edit_user->setFocus();
    }

}

