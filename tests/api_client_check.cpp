#include <QCoreApplication>
#include "service/eventservice.h"
int main(int argc,char **argv){QCoreApplication app(argc,argv);EventService service;
 EventResult result=service.queryLatest("does-not-exist.local.json",false,QDate(),1,10);
 if(!result.error.isEmpty() || result.total<1)return 1;
 EventResult empty=service.queryLatest(QString(),true,QDate(1900,1,1),1,10);
 if(!empty.error.isEmpty() || empty.total!=0)return 2;
 qputenv("NEBULAVISION_API_URL","http://127.0.0.1:18881");
 EventResult offline=service.queryLatest();if(offline.error.isEmpty())return 3;
 return 0;}
