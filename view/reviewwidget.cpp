#include "reviewwidget.h"
#include "../service/apiclient.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QDir>
#include <QUuid>
#include <QTimer>
#include <QJsonArray>
ReviewWidget::ReviewWidget(QWidget *parent,const QString &configPath):QWidget(parent) {
    Q_UNUSED(configPath);qRegisterMetaType<RecordPage>("RecordPage");
    QVBoxLayout *layout=new QVBoxLayout(this);
    layout->addWidget(new QLabel("录像与事件查询（服务端API）"));
    QHBoxLayout *filters=new QHBoxLayout;
    typeBox=new QComboBox;typeBox->addItems({"录像记录（新库）","异常事件（新库）"});typeBox->setCurrentIndex(1);
    datePicker=new DataPicker;datePicker->setFixedWidth(230);
    QPushButton *query=new QPushButton("查询日期"),*all=new QPushButton("全部记录");
    filters->addWidget(typeBox);filters->addWidget(datePicker);filters->addWidget(query);filters->addWidget(all);
    QPushButton *detect=new QPushButton("检测所选录像");filters->addWidget(detect);connect(detect,&QPushButton::clicked,this,&ReviewWidget::detectSelected);filters->addStretch();layout->addLayout(filters);
    table=new QTableWidget;table->setObjectName("recordTable");table->setEditTriggers(QAbstractItemView::NoEditTriggers);table->setSelectionBehavior(QAbstractItemView::SelectRows);table->setAlternatingRowColors(true);table->setSortingEnabled(false);table->verticalHeader()->hide();layout->addWidget(table,1);
    QHBoxLayout *pagination=new QHBoxLayout;
    count=new QLabel("登录后进入回看页查询");count->setObjectName("review_info");previous=new QPushButton("上一页");next=new QPushButton("下一页");jump=new QSpinBox;pageSize=new QComboBox;pageSize->addItems({"10","20","50"});jump->setRange(1,1);
    previous->setObjectName("previousPage");next->setObjectName("nextPage");jump->setObjectName("pageNumber");pageSize->setObjectName("pageSize");
    pagination->addWidget(count);pagination->addStretch();pagination->addWidget(new QLabel("每页"));pagination->addWidget(pageSize);pagination->addWidget(previous);pagination->addWidget(jump);pagination->addWidget(next);layout->addLayout(pagination);
    detail=new QLabel("双击ready录像，通过授权接口下载后在监控页播放。");detail->setObjectName("review_detail");detail->setWordWrap(true);layout->addWidget(detail);previous->setEnabled(false);next->setEnabled(false);
    connect(query,&QPushButton::clicked,this,[this](){resetQuery(true);});connect(all,&QPushButton::clicked,this,[this](){resetQuery(false);});
    connect(datePicker,&DataPicker::dateSelected,this,[this](const QDate &){resetQuery(true);});
    connect(typeBox,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this](){page=1;loadPage();});
    connect(pageSize,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this](){page=1;loadPage();});
    connect(previous,&QPushButton::clicked,this,[this](){if(page>1){--page;loadPage();}});connect(next,&QPushButton::clicked,this,[this](){if(page<pages){++page;loadPage();}});
    connect(jump,QOverload<int>::of(&QSpinBox::valueChanged),this,[this](int value){page=value;loadPage();});
    connect(table,&QTableWidget::cellDoubleClicked,this,[this](int row,int){openRecording(row);});
}
void ReviewWidget::refresh(){loadPage();}
void ReviewWidget::resetQuery(bool date){byDate=date;selectedDate=datePicker->getDate();page=1;loadPage();}
void ReviewWidget::loadPage(){
    ++generation;table->clear();table->setRowCount(0);previous->setEnabled(false);next->setEnabled(false);jump->setEnabled(false);
    count->setText("正在查询服务端……");detail->setText("后台HTTP请求中，界面可以继续操作。");
    if(!task)startQuery();
}
void ReviewWidget::startQuery(){
    task=new ReviewQueryTask(this);task->events=typeBox->currentIndex()==1;task->byDate=byDate;task->date=selectedDate;task->page=page;task->size=pageSize->currentText().toInt();task->requestId=generation;
    ReviewQueryTask *current=task;
    connect(current,&ReviewQueryTask::resultReady,this,&ReviewWidget::showPage,Qt::QueuedConnection);
    connect(current,&QThread::finished,this,[this,current](){bool newer=current->requestId!=generation;task=nullptr;current->deleteLater();if(newer)startQuery();});task->start();
}
void ReviewWidget::showPage(int id,const RecordPage &result){
    if(id!=generation)return;
    table->setColumnCount(result.headers.size());table->setHorizontalHeaderLabels(result.headers);
    if(!result.error.isEmpty()){count->setText("加载失败");detail->setText(result.error);return;}
    for(int row=0;row<result.rows.size();row++){
        table->insertRow(row);for(int col=0;col<result.rows[row].size();col++)table->setItem(row,col,new QTableWidgetItem(result.rows[row][col]));
        if(row<result.recordingIds.size() && table->item(row,0))table->item(row,0)->setData(Qt::UserRole,result.recordingIds[row]);
    }
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    table->horizontalHeader()->setStretchLastSection(false);
    table->setColumnWidth(0,55);table->setColumnWidth(1,150);table->setColumnWidth(2,220);
    table->setColumnWidth(3,typeBox->currentIndex()==1?150:220);table->setColumnWidth(5,80);
    table->horizontalHeader()->setSectionResizeMode(4,QHeaderView::Stretch);
    table->setWordWrap(true);table->resizeRowsToContents();
    page=result.page;pages=result.pages;jump->blockSignals(true);jump->setRange(1,pages);jump->setValue(page);jump->setEnabled(result.total>0);jump->blockSignals(false);previous->setEnabled(page>1);next->setEnabled(page<pages);
    count->setText(QString("共 %1 条 · 第 %2 / %3 页").arg(result.total).arg(page).arg(pages));
    detail->setText(result.total==0?"查询成功，当前条件下没有记录。":"事件时间按UTC显示。双击ready录像可下载并回放；合成事件不代表真实检测。");
}
void ReviewWidget::openRecording(int row){
    if(!table->item(row,0) || !table->item(row,5))return;
    QString id;
    if(typeBox->currentIndex()==1){
        id=table->item(row,0)->data(Qt::UserRole).toString();
        if(id.isEmpty()){detail->setText("此事件没有关联录像。");return;}
    }else{
        if(table->item(row,5)->text()!="ready"){detail->setText("录像尚未封片或录制失败，不能播放。");return;}
        id=table->item(row,0)->text();
    }
    bool valid=false;qint64 recordingId=id.toLongLong(&valid);if(!valid || recordingId<=0){detail->setText("没有有效的录像关联");return;}
    QString root=QString::fromUtf8(qgetenv("NEBULAVISION_CACHE_ROOT"));
    if(root.isEmpty())root=QCoreApplication::applicationDirPath()+"/../../x/playback-cache";QDir().mkpath(root);
    QString localPath=QDir(root).filePath(id+".mp4");detail->setText("正在授权下载录像……");
    struct Result {QString path,error;};QFutureWatcher<Result> *watcher=new QFutureWatcher<Result>(this);
    connect(watcher,&QFutureWatcher<Result>::finished,this,[this,watcher](){Result result=watcher->result();watcher->deleteLater();if(!result.error.isEmpty()){detail->setText(result.error);return;}detail->setText("下载成功，切换到监控页播放。");emit recordingReady(result.path);});
    watcher->setFuture(QtConcurrent::run([id,localPath](){Result result;result.path=localPath;ApiClient::download("/api/recordings/file?id="+id,localPath,&result.error);return result;}));
}
ReviewWidget::~ReviewWidget(){if(task){disconnect(task,nullptr,this,nullptr);task->wait();delete task;}}

void ReviewWidget::detectSelected(){
    int row=table->currentRow();
    if(typeBox->currentIndex()!=0 || row<0 || !table->item(row,5) || table->item(row,5)->text()!="ready") {detail->setText("请切换录像列表，选择一条ready录像。");return;}
    QString recording=table->item(row,0)->text();QString jobKey="ui_"+QUuid::createUuid().toString(QUuid::WithoutBraces);
    struct Result{QJsonObject data;QString error;};QFutureWatcher<Result> *watcher=new QFutureWatcher<Result>(this);
    connect(watcher,&QFutureWatcher<Result>::finished,this,[this,watcher](){Result result=watcher->result();watcher->deleteLater();if(!result.error.isEmpty()){detail->setText(result.error);return;}latestDetection=result.data.value("id").toString();detail->setText("检测任务已提交，等待真实模型结果……");pollDetection(latestDetection,0);});
    watcher->setFuture(QtConcurrent::run([recording,jobKey](){Result result;result.data=ApiClient::request("/api/detections/start",QJsonObject{{"recordingId",recording},{"jobKey",jobKey},{"threshold",.25}},true,&result.error);return result;}));
}
void ReviewWidget::pollDetection(const QString &id,int tries){
    if(id!=latestDetection)return;
    if(tries>=20){detail->setText("检测状态等待超时，可稍后查询任务。");return;}
    struct Result{QJsonObject data;QString error;};QFutureWatcher<Result> *watcher=new QFutureWatcher<Result>(this);
    connect(watcher,&QFutureWatcher<Result>::finished,this,[this,watcher,id,tries](){
        Result result=watcher->result();watcher->deleteLater();if(id!=latestDetection)return;
        if(!result.error.isEmpty()){detail->setText(result.error);return;}
        QString status=result.data.value("status").toString();
        if(status=="queued" || status=="running"){QTimer::singleShot(500,this,[this,id,tries](){pollDetection(id,tries+1);});return;}
        if(status!="succeeded"){detail->setText("检测失败："+result.data.value("error").toString());return;}
        QJsonObject detection=result.data.value("result").toObject();
        detail->setText(QString("真实检测完成：区域内人员 %1 个，事件ID %2。切换事件列表可查看并双击证据回放。").arg(detection.value("detections").toArray().size()).arg(detection.value("eventId").toString("无新事件")));
    });
    watcher->setFuture(QtConcurrent::run([id](){Result result;result.data=ApiClient::request("/api/detections/job?id="+id,{},false,&result.error);return result;}));
}
