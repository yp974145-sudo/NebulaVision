#include <QApplication>
#include <QDebug>
#include "service/eventservice.h"
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    EventService service;
    EventResult first = service.queryLatest(QString(),true,QDate(2030,1,1),1,10);
    if (!first.error.isEmpty() || first.total!=25 || first.events.size()!=10 || first.pages!=3) return 1;
    EventResult last = service.queryLatest(QString(),true,QDate(2030,1,1),99,10);
    if (!last.error.isEmpty() || last.page!=3 || last.events.size()!=5) return 2;
    EventResult empty = service.queryLatest(QString(),true,QDate(1900,1,1),1,10);
    if (!empty.error.isEmpty() || empty.total!=0 || empty.pages!=1) return 3;
    EventResult invalid = service.queryLatest(QString(),true,QDate(),1,10);
    if (invalid.error.isEmpty()) return 4;
    EventResult nextDay = service.queryLatest(QString(),true,QDate(2030,1,2),1,10);
    if (!nextDay.error.isEmpty() || nextDay.total!=1) return 5;
    EventResult size = service.queryLatest(QString(),true,QDate(2030,1,1),-3,0);
    if (!size.error.isEmpty() || size.page!=1 || size.events.size()!=1) return 6;
    if (first.events.first().occurredAt!="2030-01-01 23:59:59.999") return 7;
    return 0;
}
