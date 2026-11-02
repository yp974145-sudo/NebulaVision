# Debug链接排查与输出隔离

2026-11-02。用户提供collect2和Makefile.Debug汇总错误，没有提供前面的具体链接诊断。
直接使用原Qt Creator构建目录重新链接成功，因此没有复现持续缺库或undefined reference；不能断言原错误必然是文件占用。
发现Debug和Release共用bin-qt6/class15.exe，已将Qt6 Debug目标改为class15_debug，Release仍为class15。
重新运行qmake、构建Makefile.Debug通过，class15_debug.exe --check-dev-db退出码0。
用户后续应先停止运行中的程序，再运行qmake并重新构建；Debug文件运行期间再次链接它仍可能被Windows拒绝。
若仍出现collect2汇总错误，应读取“编译输出”前面的cannot open output file、Permission denied或undefined reference等实际诊断。
