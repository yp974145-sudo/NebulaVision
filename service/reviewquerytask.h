#ifndef REVIEWQUERYTASK_H
#define REVIEWQUERYTASK_H
#include <QThread>
#include "../model/recordmodel.h"
#include <QMetaType>
Q_DECLARE_METATYPE(RecordPage)
class ReviewQueryTask : public QThread {
    Q_OBJECT
public:
    bool events,byDate;QDate date;int page,size,requestId;
    explicit ReviewQueryTask(QObject *parent=nullptr):QThread(parent){}
signals:
    void resultReady(int requestId,RecordPage result);
protected:
    void run() override;
};
#endif
