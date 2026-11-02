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
class ReviewWidget : public QWidget {
    Q_OBJECT
public:
    explicit ReviewWidget(QWidget *parent=nullptr);
    void refresh();
private:
    void loadPage();
    void resetQuery(bool byDate);
    DataPicker *datePicker;
    QComboBox *typeBox, *pageSize;
    QTableWidget *table;
    QPushButton *previous, *next;
    QSpinBox *jump;
    QLabel *count, *detail;
    RecordService service;
    bool byDate=false;
    QDate selectedDate;
    int page=1, pages=1;
};
#endif