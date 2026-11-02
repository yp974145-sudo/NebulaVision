#ifndef WINMANAGER_H
#define WINMANAGER_H
#include "../view/mainwidget.h"
#include "../view/loginwidget.h"
#include "../view/registerwidget.h"
class WinManager
{
public:
    static WinManager *getInstence();
    static void releaseInstence();
    WinManager(const WinManager &)=delete;
    WinManager &operator=(const WinManager &)=delete;
    MainWidget mainWidget;
    LoginWidget loginWidget;
    RegisterWidget registerWidget;
    void showReview();
private:
    WinManager();
    ~WinManager()=default;
    static WinManager *instance;
};
#endif
