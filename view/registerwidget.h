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
#include <QListWidget>
class RegisterWidget : public QWidget
{
    Q_OBJECT
public:
    explicit RegisterWidget(QWidget *parent=nullptr);
    void init_qss();
private:
    QLineEdit *username;
    QLineEdit *password;
    QLineEdit *confirmation;
    QListWidget *cameras;
    QPushButton *create;
    QLabel *status;
    int generation=0;
protected:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;
};
#endif
