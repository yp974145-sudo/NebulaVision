#ifndef DATAPICKER_H
#define DATAPICKER_H

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QCalendarWidget>
#include <QFrame>
#include <QDate>
#include <QDebug>
#include <QScreen>
#include <QEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGuiApplication>

class DataPicker : public QWidget
{
    Q_OBJECT
private:
    QLineEdit *m_lineEdit;    //日期显示输入框
    QPushButton *m_btn;       //切换日历控件显示隐藏的按钮
    QFrame *m_calendarFram;   //日历的容器框架
    QCalendarWidget *m_calendar; //日历控件
    QString m_displayFormat;  //日期显示格式
public:
    explicit DataPicker(QWidget *parent = nullptr);
    ~DataPicker() override;
    void setDate(const QDate &date);//设置日期
    QDate getDate()const;//获取选中的日期
    void setDisplayFormat(const QString &format);//设置日期显示格式
protected:
    bool eventFilter(QObject *obj, QEvent *event) override;//事件过滤器
signals:
    void dateSelected(const QDate &date);

public slots:
    void onDateSelected(const QDate &date);//日历控件中选中日期后的槽函数
    void toggleCalendarVisibility();   //切换日历显示/隐藏
};

#endif // DATAPICKER_H
