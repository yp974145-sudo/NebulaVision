#ifndef REVIEWWIDGET_H
#define REVIEWWIDGET_H
#include <QWidget>
#include <QTableWidget>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QSpinBox>
#include "datapicker.h"
#include "../service/reviewquerytask.h"
class ReviewWidget : public QWidget {
    Q_OBJECT
public:
    explicit ReviewWidget(QWidget *parent=nullptr,const QString &configPath=QString());
    ~ReviewWidget() override;
    void refresh();
signals:
    void recordingReady(const QString &localPath);
private:
    void resetQuery(bool date);
    void loadPage();
    void startQuery();
    void showPage(int id,const RecordPage &result);
    void openRecording(int row);
    void detectSelected();
    void pollDetection(const QString &id,int tries);
    QString latestDetection;
    DataPicker *datePicker;
    QComboBox *typeBox,*pageSize;
    QTableWidget *table;
    QPushButton *previous,*next;
    QSpinBox *jump;
    QLabel *count,*detail;
    bool byDate=false;
    QDate selectedDate;
    int page=1,pages=1,generation=0;
    ReviewQueryTask *task=nullptr;
};
#endif
