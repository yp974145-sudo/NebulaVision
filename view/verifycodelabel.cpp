#include "verifycodelabel.h"

#include <QPainter>
#include <QMouseEvent>
#include <QDateTime>
#include <QFont>
#include <QRect>
#include <random>

namespace {

// 整个程序共用一份随机数引擎，用毫秒时间戳做种子
std::mt19937 &rng()
{
    static std::mt19937 engine(static_cast<unsigned>(QDateTime::currentMSecsSinceEpoch()));
    return engine;
}

// 返回 [low, high] 之间的随机整数
int randInt(int low, int high)
{
    std::uniform_int_distribution<int> dist(low, high);
    return dist(rng());
}

}

VerifyCodeLabel::VerifyCodeLabel(QWidget *parent) : QWidget(parent)
{
    setFixedSize(100, 36);
    setCursor(Qt::PointingHandCursor);
    setToolTip("看不清？点一下换一张");
    refresh();
}

void VerifyCodeLabel::refresh()
{
    // 去掉容易看混的 0 O 1 I L
    static const QString chars = QStringLiteral("23456789ABCDEFGHJKLMNPQRSTUVWXYZ");
    m_code.clear();
    for (int i = 0; i < kCodeLength; ++i)
        m_code.append(chars.at(randInt(0, chars.size() - 1)));
    update();   // 请求重画
}

void VerifyCodeLabel::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        refresh();
        emit clicked();
    }
    QWidget::mousePressEvent(event);
}

void VerifyCodeLabel::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // 1. 底色和边框，跟输入框保持一个风格
    QRect box = rect().adjusted(0, 0, -1, -1);
    p.setPen(QPen(QColor("#d0d0d0")));
    p.setBrush(QColor("#fdfdfd"));
    p.drawRoundedRect(box, 6, 6);

    // 2. 干扰线
    for (int i = 0; i < 4; ++i) {
        p.setPen(QPen(QColor(randInt(120, 200), randInt(120, 200), randInt(120, 200)), 1));
        p.drawLine(randInt(2, width() - 3), randInt(2, height() - 3),
                   randInt(2, width() - 3), randInt(2, height() - 3));
    }

    // 3. 干扰点
    for (int i = 0; i < 40; ++i) {
        p.setPen(QColor(randInt(130, 210), randInt(130, 210), randInt(130, 210)));
        p.drawPoint(randInt(2, width() - 3), randInt(2, height() - 3));
    }

    // 4. 四个字符，每个歪一点、颜色深一点
    const int cellW = (width() - 8) / kCodeLength;
    for (int i = 0; i < m_code.size(); ++i) {
        p.save();
        p.setFont(QFont(QStringLiteral("Consolas"), 14, QFont::Bold));
        p.setPen(QColor(randInt(20, 110), randInt(20, 110), randInt(90, 170)));

        const int x = 4 + i * cellW + randInt(0, 4);
        const int y = height() / 2 + randInt(-3, 3);
        p.translate(x + cellW / 2, y);
        p.rotate(randInt(-28, 28));
        p.drawText(QRect(-cellW / 2, -height() / 2, cellW, height()),
                   Qt::AlignCenter, QString(m_code.at(i)));
        p.restore();
    }
}
