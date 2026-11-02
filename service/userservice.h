#ifndef USERSERVICE_H
#define USERSERVICE_H
#include "../model/usermodel.h"
class UserService {
public:
    bool login(QString username, QString password);
    QString lastError() const { return error; }
private:
    UserModel userModel;
    QString error;
};
#endif
