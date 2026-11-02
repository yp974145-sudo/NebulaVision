#ifndef REVIEWWIDGET_H
#define REVIEWWIDGET_H
#include <QWidget>
#include <QTableWidget>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QSpinBox>
#include "datapicker.h"
#include "../service/recordservice.h"
#include "../service/eventservice.h"
class ReviewWidget : public QWidget {
    Q_OBJECT
public:
    explicit ReviewWidget(QWidget *parent=nullptr, const QString &configPath=QString());
    void refresh();
private:
    void loadPage();
    void loadEvents();
    void resetQuery(bool byDate);
    DataPicker *datePicker;
    QComboBox *typeBox, *pageSize;
    QTableWidget *table;
    QPushButton *previous, *next;
    QSpinBox *jump;
    QLabel *count, *detail;
    RecordService service;
    EventService eventService;
    QString eventConfigPath; // 留空时读取默认本地配置；测试时可指定配置副本。
    bool byDate=false;
    QDate selectedDate;
    int page=1, pages=1;
};
#endif