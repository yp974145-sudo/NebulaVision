#include "reviewwidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QDesktopServices>
#include <QFileInfo>
#include <QUrl>
#include <QMessageBox>
ReviewWidget::ReviewWidget(QWidget *parent) : QWidget(parent)
{
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(16,16,16,16);
    layout->setSpacing(14);
    auto title = new QLabel("录像与事件查询");
    title->setObjectName("sectionTitle");
    layout->addWidget(title);
    auto filters = new QHBoxLayout;
    datePicker = new DataPicker;
    datePicker->setFixedWidth(230);
    typeBox = new QComboBox;
    typeBox->addItems({"录像记录","异常事件"});
    auto query = new QPushButton("查询日期");
    auto all = new QPushButton("全部记录");
    filters->addWidget(typeBox);
    filters->addWidget(datePicker);
    filters->addWidget(query);
    filters->addWidget(all);
    filters->addStretch();
    layout->addLayout(filters);
    table = new QTableWidget;
    table->setObjectName("recordTable");
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setAlternatingRowColors(true);
    table->verticalHeader()->hide();
    // 顺序由数据库统一排好；只在当前页排序，会让整份列表的顺序对不上。
    table->setSortingEnabled(false);
    layout->addWidget(table,1);
    // 分页操作放在表格下面：每页条数、上一页、跳页、下一页。
    auto pagination = new QHBoxLayout;
    count = new QLabel("进入回看页后加载记录");
    previous = new QPushButton("上一页");
    next = new QPushButton("下一页");
    previous->setObjectName("previousPage");
    next->setObjectName("nextPage");
    jump = new QSpinBox;
    jump->setObjectName("pageNumber");
    jump->setRange(1,1);
    pageSize = new QComboBox;
    pageSize->setObjectName("pageSize");
    pageSize->addItems({"10","20","50"});
    pagination->addWidget(count);
    pagination->addStretch();
    pagination->addWidget(new QLabel("每页"));
    pagination->addWidget(pageSize);
    pagination->addWidget(previous);
    pagination->addWidget(jump);
    pagination->addWidget(next);
    layout->addLayout(pagination);
    detail = new QLabel("双击记录可打开本机视频文件；不存在的文件会提示路径。");
    detail->setWordWrap(true);
    layout->addWidget(detail);
    previous->setEnabled(false);
    next->setEnabled(false);
    connect(query,&QPushButton::clicked,this,[this]{ resetQuery(true); });
    connect(all,&QPushButton::clicked,this,[this]{ resetQuery(false); });
    connect(datePicker,&DataPicker::dateSelected,this,[this](const QDate &){ resetQuery(true); });
    // 换类型或每页条数时，从第一页重新查，日期筛选条件仍然保留。
    connect(typeBox,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this]{ page=1; loadPage(); });
    connect(pageSize,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this]{ page=1; loadPage(); });
    connect(previous,&QPushButton::clicked,this,[this]{ if(page>1) { --page; loadPage(); } });
    connect(next,&QPushButton::clicked,this,[this]{ if(page<pages) { ++page; loadPage(); } });
    connect(jump,QOverload<int>::of(&QSpinBox::valueChanged),this,[this](int value){ page=value; loadPage(); });
    connect(table,&QTableWidget::cellDoubleClicked,this,[this](int row,int){
        auto item=table->item(row,table->columnCount()-1);
        if(!item) return;
        QString path=item->text();
        detail->setText("路径："+path);
        if (!QFileInfo(path).isFile()) { QMessageBox::information(this,"视频文件","本机找不到该文件：\n"+path); return; }
        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absoluteFilePath())))
            QMessageBox::warning(this,"打开失败","请安装能够打开该视频格式的播放器。");
    });
}
void ReviewWidget::refresh() { loadPage(); }
void ReviewWidget::resetQuery(bool enabled)
{
    // 点“全部记录”时关闭日期筛选；点日期查询时，只看选中的那一天。
    byDate=enabled; selectedDate=datePicker->getDate(); page=1; loadPage();
}
void ReviewWidget::loadPage()
{
    // View 只把查询条件交给 Service，再把返回的这一页显示出来。
    auto result=service.query(typeBox->currentIndex()==1,byDate,selectedDate,page,pageSize->currentText().toInt());
    // 先清掉旧数据，查询失败时就不会还显示上一次的结果。
    table->clear();
    table->setRowCount(0);
    detail->setText("双击记录可打开本机视频文件。");
    // 下面会用代码调整页码，先暂停信号，免得又触发一次 loadPage。
    jump->blockSignals(true);
    if (!result.error.isEmpty()) {
        count->setText("加载失败");
        detail->setText(result.error);
        page=pages=1;
        jump->setRange(1,1); jump->setValue(1); jump->setEnabled(false);
        previous->setEnabled(false); next->setEnabled(false);
        jump->blockSignals(false);
        return;
    }
    table->setColumnCount(result.headers.size());
    table->setHorizontalHeaderLabels(result.headers);
    table->setRowCount(result.rows.size());
    for(int r=0;r<result.rows.size();++r)
        for(int c=0;c<result.rows[r].size();++c)
            table->setItem(r,c,new QTableWidgetItem(result.rows[r][c]));
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table->horizontalHeader()->setStretchLastSection(true);
    page=result.page; pages=result.pages;
    jump->setRange(1,pages); jump->setValue(page); jump->setEnabled(result.total>0);
    jump->blockSignals(false);
    // 第一页不能再往前，最后一页不能再往后。
    previous->setEnabled(page>1); next->setEnabled(page<pages);
    count->setText(QString("共 %1 条 · 第 %2 / %3 页").arg(result.total).arg(page).arg(pages));
    if(result.total==0) detail->setText("当前条件下没有记录，可以更换日期或查看全部记录。");
}
