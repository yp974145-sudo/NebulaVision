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
#include "../service/eventquerytask.h"
class ReviewWidget : public QWidget {
    Q_OBJECT
public:
    explicit ReviewWidget(QWidget *parent=nullptr, const QString &configPath=QString());
    void refresh();
    ~ReviewWidget() override;
private:
    void loadPage();
    void loadEvents();
    void startEventQuery();
    void showEvents(int requestId, const EventResult &result);
    void resetQuery(bool byDate);
    DataPicker *datePicker;
    QComboBox *typeBox, *pageSize;
    QTableWidget *table;
    QPushButton *previous, *next;
    QSpinBox *jump;
    QLabel *count, *detail;
    RecordService service;
    EventQueryTask *eventTask = nullptr;
    int eventRequestId = 0;
    QString eventConfigPath; // 留空时读取默认本地配置；测试时可指定配置副本。
    bool byDate=false;
    QDate selectedDate;
    int page=1, pages=1;
};
#endif