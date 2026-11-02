#ifndef USERMODEL_H
#define USERMODEL_H
#include <QString>
class UserModel {
public:
    bool queryUserAndPwd(QString &username, QString &password, int &userId);
    int updateLoginTime(int &userId);
    QString lastError() const { return error; }
private:
    QString error;
};
#endif
