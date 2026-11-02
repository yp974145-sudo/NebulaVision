#ifndef VERIFYCODELABEL_H
#define VERIFYCODELABEL_H

#include <QWidget>
#include <QString>

// 验证码控件：自己画出 4 位随机字符，带干扰线干扰点，点一下就换一张
class VerifyCodeLabel : public QWidget
{
    Q_OBJECT
public:
    explicit VerifyCodeLabel(QWidget *parent = nullptr);

    // 当前这一张的验证码，登录时拿它跟输入框比对
    QString code() const { return m_code; }

    // 重新生成一张
    void refresh();

signals:
    // 被点击时发出（点击本身已经刷新了画面）
    void clicked();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    QString m_code;
    static const int kCodeLength = 4;
};

#endif // VERIFYCODELABEL_H
