#include "userservice.h"
// Service 把登录步骤串起来：检查输入、验证账号、更新登录时间。
// 窗口只需要拿到成功或失败的结果，不用在窗口里写 SQL。
bool UserService::login(QString username, QString password)
{
    error.clear();
    username = username.trimmed();
    if (username.isEmpty() || password.isEmpty()) { error = "请输入用户名和密码"; return false; }
    int id = -1;
    if (!userModel.queryUserAndPwd(username, password, id)) {
        error = userModel.lastError().isEmpty() ? "用户名或密码错误，或账号已停用" : userModel.lastError();
        return false;
    }
    // 同一秒内重复登录，时间没变时 MySQL 可能返回 0，这种情况也算成功。
    if (userModel.updateLoginTime(id) < 0) { error = userModel.lastError(); return false; }
    return true;
}
