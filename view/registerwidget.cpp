#include "registerwidget.h"
#include "../util/winmanager.h"
#include "../service/apiclient.h"
#include <QCloseEvent>
#include <QShowEvent>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QJsonArray>
RegisterWidget::RegisterWidget(QWidget *parent):QWidget(parent) {
    setWindowTitle("创建普通用户");resize(560,600);
    QVBoxLayout *layout=new QVBoxLayout(this);QFormLayout *form=new QFormLayout;
    username=new QLineEdit;password=new QLineEdit;confirmation=new QLineEdit;
    username->setObjectName("create_username");password->setObjectName("create_password");confirmation->setObjectName("create_confirmation");
    password->setEchoMode(QLineEdit::Password);confirmation->setEchoMode(QLineEdit::Password);
    form->addRow("用户名",username);form->addRow("密码",password);form->addRow("确认密码",confirmation);layout->addLayout(form);
    layout->addWidget(new QLabel("勾选该用户可以访问的设备"));cameras=new QListWidget;layout->addWidget(cameras);
    status=new QLabel;status->setWordWrap(true);layout->addWidget(status);
    create=new QPushButton("创建账号");create->setObjectName("create_account");layout->addWidget(create);
    QPushButton *back=new QPushButton("返回主界面");layout->addWidget(back);connect(back,&QPushButton::clicked,this,&QWidget::close);
    connect(create,&QPushButton::clicked,this,[this](){
        if(username->text().trimmed().isEmpty() || password->text().size()<6 || password->text()!=confirmation->text()){
            status->setText("请输入用户名和至少6位密码，两次密码须一致。");return;
        }
        QJsonArray ids;for(int i=0;i<cameras->count();i++)if(cameras->item(i)->checkState()==Qt::Checked)ids.append(cameras->item(i)->data(Qt::UserRole).toString());
        QJsonObject body{{"username",username->text().trimmed()},{"password",password->text()},{"cameraIds",ids}};
        int current=generation;create->setEnabled(false);password->clear();confirmation->clear();
        struct Result {QJsonObject data;QString error;};auto watcher=new QFutureWatcher<Result>(this);
        connect(watcher,&QFutureWatcher<Result>::finished,this,[this,watcher,current](){
            Result r=watcher->result();watcher->deleteLater();if(current!=generation)return;create->setEnabled(true);
            status->setText(r.error.isEmpty()?"账号创建成功，可以返回后使用新账号登录。":r.error);
        });
        watcher->setFuture(QtConcurrent::run([body](){Result r;r.data=ApiClient::request("/api/users",body,true,&r.error);return r;}));
    });init_qss();
}
void RegisterWidget::init_qss() {QFile file(":/qss/login_reg.qss");if(file.open(QIODevice::ReadOnly))setStyleSheet(file.readAll());}
void RegisterWidget::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);int current=++generation;create->setEnabled(false);cameras->clear();password->clear();confirmation->clear();status->setText("正在检查管理员权限……");
    struct Result {QJsonObject data;QString error;};auto watcher=new QFutureWatcher<Result>(this);
    connect(watcher,&QFutureWatcher<Result>::finished,this,[this,watcher,current](){
        Result r=watcher->result();watcher->deleteLater();if(current!=generation)return;
        if(!r.error.isEmpty()){status->setText(r.error);return;}
        for(const QJsonValue &value:r.data.value("cameras").toArray()){
            QJsonObject camera=value.toObject();auto item=new QListWidgetItem(camera.value("name").toString(),cameras);
            item->setData(Qt::UserRole,camera.value("id").toString());item->setFlags(item->flags()|Qt::ItemIsUserCheckable);item->setCheckState(Qt::Unchecked);
        }
        create->setEnabled(true);status->setText("用户名使用3至32位字母、数字或下划线；未勾选设备则没有设备访问权限。");
    });
    watcher->setFuture(QtConcurrent::run([](){Result r;auto me=ApiClient::request("/api/me",{},false,&r.error);
        if(r.error.isEmpty() && !me.value("admin").toBool())r.error="只有管理员可以创建账号。";
        if(r.error.isEmpty())r.data=ApiClient::request("/api/cameras",{},false,&r.error);return r;}));
}
void RegisterWidget::closeEvent(QCloseEvent *event) {++generation;password->clear();confirmation->clear();WinManager::getInstence()->mainWidget.show();event->accept();}
