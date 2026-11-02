#include <QApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QElapsedTimer>
#include <QThread>
#include <QDebug>
#include "view/mainwidget.h"
#include "service/apiclient.h"
void spinDetect(int ms){QElapsedTimer time;time.start();while(time.elapsed()<ms){QApplication::processEvents();QThread::msleep(10);}}
int main(int argc,char **argv){QApplication app(argc,argv);QFile accounts(app.arguments().at(1));if(!accounts.open(QIODevice::ReadOnly))return 1;
 QString error;QJsonObject admin=QJsonDocument::fromJson(accounts.readAll()).object().value("admin").toObject();QJsonObject login=ApiClient::request("/api/login",admin,true,&error);if(!error.isEmpty())return 2;ApiSession::setToken(login.value("token").toString());
 MainWidget window;window.setAttribute(Qt::WA_DontShowOnScreen);window.resize(1100,760);window.show();window.showUsername("API demo");window.switch_to_review();
 ReviewWidget *review=window.findChild<ReviewWidget *>();QTableWidget *table=review->findChild<QTableWidget *>("recordTable");
 QElapsedTimer wait;wait.start();int row=-1;while(wait.elapsed()<10000){spinDetect(20);for(int i=0;i<table->rowCount();i++)if(table->item(i,0) && table->item(i,0)->data(Qt::UserRole).toString().toLongLong()>0)row=i;if(row>=0)break;}
 if(row<0)return 3;qInfo()<<"evidence id"<<table->item(row,0)->data(Qt::UserRole).toString();window.grab().save(app.arguments().at(2)+"/events-api.png");
 FfmpegPlayer *player=window.findChild<FfmpegPlayer *>("previewPlayer0");int frames=0;QObject::connect(player,&FfmpegPlayer::frameDisplayed,[&frames](){++frames;});
 QMetaObject::invokeMethod(table,"cellDoubleClicked",Qt::DirectConnection,Q_ARG(int,row),Q_ARG(int,0));wait.restart();while(wait.elapsed()<10000 && frames==0)spinDetect(20);if(frames==0){for(QLabel *label:window.findChildren<QLabel *>())qWarning()<<label->text();return 4;}
 window.grab().save(app.arguments().at(2)+"/event-evidence-playback.png");
 // 在原窗口选择ready录像并点击真实检测按钮，验证任务完成提示。
 window.switch_to_review();QList<QComboBox *> boxes=review->findChildren<QComboBox *>();for(QComboBox *box:boxes)if(box->itemText(0).contains("录像"))box->setCurrentIndex(0);
 wait.restart();row=-1;while(wait.elapsed()<10000){spinDetect(20);for(int i=0;i<table->rowCount();i++)if(row<0 && table->item(i,5)&&table->item(i,5)->text()=="ready")row=i;if(row>=0)break;}
 if(row<0)return 5;table->selectRow(row);QList<QPushButton *> buttons=review->findChildren<QPushButton *>();for(QPushButton *button:buttons)if(button->text()=="检测所选录像")button->click();
 bool done=false;wait.restart();while(wait.elapsed()<12000){spinDetect(20);for(QLabel *label:review->findChildren<QLabel *>())if(label->text().contains("真实检测完成"))done=true;if(done)break;}
 if(!done)return 6;
 window.grab().save(app.arguments().at(2)+"/detection-ui.png");
 window.resize(1000,700);spinDetect(50);qInfo()<<"final window"<<window.size()<<"minimum"<<window.minimumSizeHint();
 return 0;}
