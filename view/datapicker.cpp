#include "datapicker.h"

DataPicker::DataPicker(QWidget *parent) : QWidget(parent)
{
    this->m_displayFormat="yyyy-MM-dd";

    //1.输入框初始化
    m_lineEdit=new QLineEdit(this);
    m_lineEdit->setReadOnly(true);
    m_lineEdit->setPlaceholderText("请选择日期");
    m_lineEdit->setCursor(Qt::PointingHandCursor);//切换为鼠标手

    //2.按钮初始化
    m_btn=new QPushButton(this);
    m_btn->setText("...");

    //3,日历控件初始化
    m_calendarFram=new QFrame(nullptr, Qt::Popup);
    m_calendarFram->setFrameShape(QFrame::StyledPanel);//设置样式
    m_calendarFram->setFrameShadow(QFrame::Raised);//立体阴影
    m_calendarFram->setVisible(false);

    //4.日期控件初始化
    m_calendar=new QCalendarWidget(m_calendarFram);
    m_calendar->setGridVisible(true);
    m_calendar->setMinimumDate(QDate(1900,1,1));
    m_calendar->setMaximumDate(QDate(2100,12,31));

    //5.设置默认显示日期
    setDate(QDate::currentDate());

    //6.布局初始化
    QHBoxLayout *mainLayout=new QHBoxLayout(this);
    mainLayout->addWidget(m_lineEdit);
    mainLayout->addWidget(m_btn);
    mainLayout->setContentsMargins(0,0,0,0);
    QVBoxLayout *calendarLayout=new QVBoxLayout(m_calendarFram);
    calendarLayout->addWidget(m_calendar);
    calendarLayout->setContentsMargins(2,2,2,2);

    connect(m_btn,SIGNAL(clicked()),this,SLOT(toggleCalendarVisibility()));
    connect(m_calendar,SIGNAL(clicked(QDate)),this,SLOT(onDateSelected(QDate)));
    m_lineEdit->installEventFilter(this);
}

void DataPicker::setDate(const QDate &date)
{
    if(!date.isValid()) return;
    m_calendar->setSelectedDate(date);
    m_lineEdit->setText(date.toString(m_displayFormat));
}

QDate DataPicker::getDate()const
{
    return m_calendar->selectedDate();
}

void DataPicker::setDisplayFormat(const QString &format)
{
    this->m_displayFormat=format;
    m_lineEdit->setText(getDate().toString(format));
}

DataPicker::~DataPicker() { delete m_calendarFram; }

bool DataPicker::eventFilter(QObject *obj, QEvent *event)
{
    if(obj==m_lineEdit&&event->type()==QEvent::MouseButtonPress){
        toggleCalendarVisibility();
        return true;
    }
    return QObject::eventFilter(obj,event);
}

void DataPicker::onDateSelected(const QDate &date)
{
    setDate(date);
    m_calendarFram->setVisible(false);
    emit dateSelected(date);

}

void DataPicker::toggleCalendarVisibility()
{
    if(m_calendarFram->isVisible()){
        m_calendarFram->hide();

    }else {
        //1计算默认位置，输入框正下方
        QPoint pos=mapToGlobal(m_lineEdit->geometry().bottomLeft());

        //2强制刷新日历大小避免太小看不到
        m_calendarFram->adjustSize();//让框架自适应日历控件大小
        QSize calendarSize=m_calendarFram->size();

        //3屏幕边界检查，避免超出屏幕
        QRect screenRect=QGuiApplication::primaryScreen()->availableGeometry();
        //若超出屏幕底部显示在输入框上方
        if(pos.y()+calendarSize.height()>screenRect.bottom()){
            pos.setY(mapToGlobal(m_lineEdit->geometry().topLeft()).y()-calendarSize.height());
        }
        //若超出屏幕右侧，则左对齐
        if(pos.x()+calendarSize.width()>screenRect.right()){
            pos.setX(screenRect.right()-calendarSize.width());
        }
        //确保不超过屏幕左上角
        pos.setX(qMax(screenRect.left(),pos.x()));
        pos.setY(qMax(screenRect.top(),pos.y()));
        //移动日历控件到上述位置，并显示
        m_calendarFram->move(pos);
        m_calendarFram->show();
    }
}
