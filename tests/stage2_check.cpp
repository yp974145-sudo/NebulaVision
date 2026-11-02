#include <QApplication>
#include <QElapsedTimer>
#include <QThread>
#include <QTableWidget>
#include <QComboBox>
#include <QPushButton>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QDebug>
#include "view/reviewwidget.h"
#include "service/eventservice.h"

bool waitForQuery(ReviewWidget &widget) {
    QElapsedTimer time;time.start();
    while(time.elapsed()<8000) {
        QApplication::processEvents();
        QList<QLabel *> labels=widget.findChildren<QLabel *>();
        bool loading=false;
        for(int i=0;i<labels.size();i++)if(labels[i]->text().contains("正在查询"))loading=true;
        if(!loading)return true;
        QThread::msleep(10);
    }
    return false;
}
int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    EventService service;
    EventResult expected = service.queryLatest(QString(),false,QDate(),1,10);
    if (!expected.error.isEmpty() || expected.events.isEmpty()) return 1;

    // 验证原页面确实展示新库结果，而非仅验证仓储能查询。
    ReviewWidget widget;
    widget.resize(1200, 650);
    widget.refresh();
    if(!waitForQuery(widget)) return 12;
    QTableWidget *table = widget.findChild<QTableWidget *>("recordTable");
    if (!table || table->columnCount() != 6) return 2;
    if (table->rowCount() != expected.events.size()) return 3;
    if (table->item(0, 4)->text() != expected.events.first().description) return 4;
    if (widget.findChild<QPushButton *>("nextPage")->isEnabled()) return 5;
    widget.setAttribute(Qt::WA_DontShowOnScreen);
    widget.show();
    app.processEvents();
    widget.grab().save("build-stage2/review.png");

    // 通过独立配置副本测试错误分支，不改真实配置，不输出密码。
    QDir directory(QCoreApplication::applicationDirPath());
    QString path=QString::fromUtf8(qgetenv("NEBULAVISION_DEV_CONFIG"));
    if(path.isEmpty())path=directory.filePath("../NebulaVision/config/viewer.local.json");
    QFile source(path);
    if (!source.open(QIODevice::ReadOnly)) return 6;
    QJsonObject config = QJsonDocument::fromJson(source.readAll()).object();
    config["password"] = "invalid-stage2-password";
    QFile wrong("build-stage2/wrong.local.json");
    if (!wrong.open(QIODevice::WriteOnly)) return 7;
    wrong.write(QJsonDocument(config).toJson());
    wrong.close();
    EventResult rejected = service.queryLatest(wrong.fileName());
    if (rejected.error.isEmpty() || !rejected.events.isEmpty()) return 8;
    EventResult missing = service.queryLatest("build-stage2/absent.local.json");
    if (missing.error.isEmpty() || !missing.events.isEmpty()) return 9;
    // 页面连接失败时必须清空表格，并展示失败提示。
    ReviewWidget failedWidget(nullptr, wrong.fileName());
    failedWidget.refresh();
    if(!waitForQuery(failedWidget))return 13;
    QTableWidget *failedTable = failedWidget.findChild<QTableWidget *>("recordTable");
    if (!failedTable || failedTable->rowCount() != 0) return 10;
    bool errorVisible = false;
    QList<QLabel *> labels = failedWidget.findChildren<QLabel *>();
    for (int i = 0; i < labels.size(); i++) {
        if (labels.at(i)->text().contains("新库连接失败")) errorVisible = true;
    }
    if (!errorVisible) return 11;
    qInfo() << "PASS: new event data, original page rendering, pagination disabled, wrong password, missing config";
    return 0;
}
