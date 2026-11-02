#ifndef MAINWIDGET_H
#define MAINWIDGET_H

#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QStackedLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QListWidgetItem>
#include <QDebug>
#include <QFile>
#include "monitorwidget.h"
#include "reviewwidget.h"
#include <QDateTime>
#include <QTimer>

class MainWidget : public QWidget
{
    Q_OBJECT
private:
    QHBoxLayout *top_layout;
    QPushButton *btn_login = nullptr;
    QPushButton *btn_logout = nullptr;
    QLabel *lab_login;
    QLabel *lay_time;
    QPushButton *btn_jiankong;
    QPushButton *btn_review;
    QStackedLayout *buttom_layout;
public:
    explicit MainWidget(QWidget *parent = nullptr);
    void inif_top_widget();
    void init_qss();
    void init_connect();
    void init_showtime();
    void showUsername(const QString &username);


signals:

public slots:
    void to_login_win();
    void update_time();
    void switch_to_monitor();
    void switch_to_review();

};

#endif // MAINWIDGET_H
