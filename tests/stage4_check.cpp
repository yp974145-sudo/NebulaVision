#include <QApplication>
#include <QElapsedTimer>
#include <QTimer>
#include <QThread>
#include <QComboBox>
#include "view/reviewwidget.h"
#include "view/datapicker.h"

bool waitForRows(ReviewWidget &widget, int rows) {
    QElapsedTimer timeout; timeout.start();
    QTableWidget *table=widget.findChild<QTableWidget *>("recordTable");
    while(timeout.elapsed()<8000) {
        QApplication::processEvents();
        bool loading=false;
        QList<QLabel *> labels=widget.findChildren<QLabel *>();
        for(int i=0;i<labels.size();i++) {
            if(labels[i]->text().contains("正在查询")) loading=true;
            if(labels[i]->text().contains("加载失败")) return false;
        }
        if(!loading && table->rowCount()==rows) return true;
        QThread::msleep(10);
    }
    return false;
}
int main(int argc,char **argv) {
    QApplication app(argc,argv);
    int ticks=0;
    QTimer pulse;
    QObject::connect(&pulse,&QTimer::timeout,[&ticks](){++ticks;});
    pulse.start(20);
    ReviewWidget widget;
    widget.refresh();
    if(!waitForRows(widget,1)) return 1;
    // 外部在查询开始前锁表约2秒，验证等待期间GUI计时器仍运行。
    if(app.arguments().contains("--locked") && ticks<20) return 2;
    DataPicker *picker=widget.findChild<DataPicker *>();
    picker->onDateSelected(QDate(1900,1,1));
    if(!waitForRows(widget,0)) return 3;
    // 先发空结果请求，立即改为演示日期，多次刷新后只能接受最新条件。
    picker->onDateSelected(QDate(1900,1,1));
    picker->onDateSelected(QDate(2026,11,1));
    for(int i=0;i<20;i++) widget.refresh();
    if(!waitForRows(widget,1)) return 4;
    ReviewWidget *closing=new ReviewWidget;
    closing->refresh();
    delete closing; // 查询期间销毁窗口，不能回调悬空窗口或留下运行中线程。
    return 0;
}
