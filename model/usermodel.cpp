#include "usermodel.h"
#include "../util/dbconn.h"
#include <QDateTime>
// Model 负责查数据库，查到的用户编号交给 Service 继续处理。
bool UserModel::queryUserAndPwd(QString &username, QString &password, int &userId)
{
    error.clear();
    userId = -1;
    QSqlDatabase db = DbConn::getInstence()->getDb();
    if (!db.isOpen() && !db.open()) { error = "数据库连接失败：" + db.lastError().text(); return false; }
    QSqlQuery query(db);
    // 输入内容通过参数绑定传进去，避免把用户名、密码直接拼进 SQL。
    // 数据库存的是 MD5 值，所以这里也要先转换再比较；status=1 表示账号可用。
    query.prepare("select id from admin_user where username=:username and password=:password and status=1");
    query.bindValue(":username", username);
    query.bindValue(":password", DbConn::getInstence()->getMd5(password));
    if (!query.exec()) { error = "查询失败：" + query.lastError().text(); return false; }
    if (!query.next()) return false; // SQL 执行成功不代表账号正确，还要看有没有匹配记录。
    userId = query.value(0).toInt();
    return true;
}
int UserModel::updateLoginTime(int &userId)
{
    // 用刚才查到的用户编号更新，避免改到其他账号。
    error.clear();
    QSqlDatabase db = DbConn::getInstence()->getDb();
    QSqlQuery query(db);
    query.prepare("update admin_user set last_login_time=:time where id=:id");
    query.bindValue(":time", QDateTime::currentDateTime());
    query.bindValue(":id", userId);
    if (!query.exec()) { error = "更新登录时间失败：" + query.lastError().text(); return -1; }
    return query.numRowsAffected(); // 返回改了几条记录；前面返回 -1 表示执行失败。
}
