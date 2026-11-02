#ifndef LOGINWIDGET_H
#define LOGINWIDGET_H

#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QDebug>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QPixmap>
#include <QBrush>
#include <QPalette>
#include <QFile>
#include <QMessageBox>
#include "verifycodelabel.h"
class QCloseEvent;
class LoginWidget : public QWidget
{
    Q_OBJECT
private:
    QPushButton* btn_login;
    QLineEdit *edit_user;
    QLineEdit *ediy_pwd;
    QLineEdit *edit_vcode;          // 验证码输入框
    VerifyCodeLabel *vcode_label;   // 验证码图片
    int loginAttempt = 0;          // 返回后，忽略旧请求稍后收到的结果。
protected:
    void closeEvent(QCloseEvent *event) override;
public:
    explicit LoginWidget(QWidget *parent = nullptr);
    void init_qss();
    void init_connect();

signals://自定义信号区，注意信号只需要声明，不需要定义
    void login_success(QString);

public slots:
    void login();
};

#endif // LOGINWIDGET_H
