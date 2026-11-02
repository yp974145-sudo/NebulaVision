#ifndef REGISTERWIDGET_H
#define REGISTERWIDGET_H
#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QPixmap>
#include <QBrush>
#include <QPalette>
#include <QFile>
class RegisterWidget : public QWidget
{
    Q_OBJECT
public:
    explicit RegisterWidget(QWidget *parent=nullptr);
    void init_qss();
protected:
    void closeEvent(QCloseEvent *event) override;
};
#endif
