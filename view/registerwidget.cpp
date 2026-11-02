#include "registerwidget.h"
#include "../util/winmanager.h"
#include <QCloseEvent>
RegisterWidget::RegisterWidget(QWidget *parent) : QWidget(parent)
{
    this->setWindowTitle("注册窗口");
    this->resize(800,600);
    QPixmap pm(":/img/e33a1b4319df49871b92f8b8cd82c6f2.png");
    QPalette pal=this->palette();
    pal.setBrush(QPalette::Window,QBrush(pm.scaled(this->width(),this->height())));
    this->setAutoFillBackground(true);
    this->setPalette(pal);
    QVBoxLayout *main_layout=new QVBoxLayout;
    this->setLayout(main_layout);
    QLabel *lab_title=new QLabel("XX系统注册窗口");
    lab_title->setFont(QFont("宋体",20,QFont::Bold));
    lab_title->setAlignment(Qt::AlignCenter);
    QHBoxLayout *h_layout=new QHBoxLayout;
    QFormLayout *form_layout=new QFormLayout;
    QLineEdit *edit_user=new QLineEdit;
    QLineEdit *edit_pwd=new QLineEdit;
    QLineEdit *edit_pwd2=new QLineEdit;
    edit_pwd->setEchoMode(QLineEdit::Password);
    edit_pwd2->setEchoMode(QLineEdit::Password);
    form_layout->addRow("用户名：",edit_user);
    form_layout->addRow("密码",edit_pwd);
    form_layout->addRow("确认密码",edit_pwd2);
    QHBoxLayout *vcode_layout=new QHBoxLayout;
    QLabel *lay_vcode=new QLabel("验证码");
    QLineEdit *edit_vcode=new QLineEdit;
    edit_vcode->setMaximumWidth(60);
    edit_vcode->setMaxLength(4);
    vcode_layout->addWidget(lay_vcode);
    vcode_layout->addWidget(edit_vcode);
    form_layout->addRow(vcode_layout);
    QHBoxLayout *btn_layout=new QHBoxLayout;
    QPushButton *btn_reg=new QPushButton("注册");
    QPushButton *btn_cancel=new QPushButton("取消");
    connect(btn_cancel,&QPushButton::clicked,this,&QWidget::close);
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
}
void RegisterWidget::init_qss()
{
    QFile file(":/qss/login_reg.qss");
    if(file.open(QFile::ReadOnly)) {
        this->setStyleSheet(file.readAll());
        file.close();
    }
}

void RegisterWidget::closeEvent(QCloseEvent *event)
{
    WinManager::getInstence()->loginWidget.show();
    event->accept();
}
