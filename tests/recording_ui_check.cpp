#include <QApplication>
#include <QElapsedTimer>
#include <QThread>
#include <QFile>
#include <QJsonDocument>
#include "view/mainwidget.h"
#include "service/apiclient.h"
void spinRecord(int ms){QElapsedTimer t;t.start();while(t.elapsed()<ms){QApplication::processEvents();QThread::msleep(10);}}
int main(int argc,char **argv){QApplication app(argc,argv);
 QFile accounts(app.arguments().at(1));if(!accounts.open(QIODevice::ReadOnly))return 1;
 QJsonObject admin=QJsonDocument::fromJson(accounts.readAll()).object().value("admin").toObject();QString error;
 QJsonObject login=ApiClient::request("/api/login",admin,true,&error);if(!error.isEmpty())return 2;ApiSession::setToken(login.value("token").toString());
 MainWidget window;window.setAttribute(Qt::WA_DontShowOnScreen);window.resize(1100,760);window.show();window.showUsername("API test");
 ReviewWidget *review=window.findChild<ReviewWidget *>();QList<QComboBox *> combos=review->findChildren<QComboBox *>();for(QComboBox *box:combos)if(box->itemText(0).contains("录像"))box->setCurrentIndex(0);
 window.switch_to_review();QTableWidget *table=review->findChild<QTableWidget *>("recordTable");int row=-1;
 QElapsedTimer wait;wait.start();while(wait.elapsed()<10000){spinRecord(20);for(int i=0;i<table->rowCount();i++)if(table->item(i,5)&&table->item(i,5)->text()=="ready")row=i;if(row>=0)break;}
 if(row<0)return 3;
 FfmpegPlayer *player=window.findChild<FfmpegPlayer *>("previewPlayer0");int frames=0;QObject::connect(player,&FfmpegPlayer::frameDisplayed,[&frames](){++frames;});
 QMetaObject::invokeMethod(table,"cellDoubleClicked",Qt::DirectConnection,Q_ARG(int,row),Q_ARG(int,0));wait.restart();while(wait.elapsed()<10000 && frames==0)spinRecord(20);
 if(frames==0)return 4;window.grab().save(app.arguments().at(2));return 0;}
