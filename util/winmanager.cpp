#include "winmanager.h"
WinManager *WinManager::instance=nullptr;
WinManager::WinManager() {}
WinManager *WinManager::getInstence()
{
    // 第一次用时创建，后面都返回同一个管理器，各窗口也就只有一份。
    if(!instance) instance=new WinManager;
    return instance;
}
void WinManager::releaseInstence()
{
    // 窗口是管理器的成员，删除管理器时会一起释放。
    delete instance;
    instance=nullptr;
}

void WinManager::showReview()
{
    // 回看页已经在主窗口的堆叠布局里，这里切过去即可，不要再 new 一个。
    mainWidget.show();
    mainWidget.switch_to_review();
    loginWidget.hide();
    registerWidget.hide();
}
