# Qt6迁移学习手册

## 迁移是什么
迁移是让现有源码、构建工具和运行依赖一起适应新Qt版本。不是只用新Qt Creator打开文件，也不是把Qt5的DLL改个名字。
本机目标是Qt6.11.2、MinGW13.1、64位、C++17。原Qt5.9.8/MinGW5.3/32位输出保留。
源码仍在class15_backup_20261102，外部文档和工具在同级x。没有修改数据库密码、数据或Qt安装目录。

## 先认识Kit
Kit是一套工具的组合：Qt版本、编译器、调试器和构建工具。
Qt Creator是编辑器，Qt是库，MinGW是编译工具。安装了新版编辑器不代表项目自动使用新版Qt。
本次选择E:/qt6/6.11.2/mingw_64和E:/qt6/Tools/mingw1310_64，不混用msvc2022_64或旧32位MinGW。

## 具体改了哪些文件
class15.pro：Qt6启用c++17、输出bin-qt6；Qt5继续c++11、输出bin。
player/qtvideoplayer.h/.cpp：按Qt主版本选择媒体接口，Qt6新增音频输出对象。
resources.qrc及9个图片文件：磁盘改英文文件名，保留中文资源别名。
.gitignore：忽略bin-qt6运行产物。
tests各pro：Qt6启用C++17并输出独立测试目录；stage5_check.cpp使用QVideoSink替代Qt5探针。
其他页面、事件SQL、业务表结构和FFmpeg进程解码逻辑没有为了迁移重写。

## 改动一 C++标准
```qmake
greaterThan(QT_MAJOR_VERSION, 5) {
    CONFIG += c++17
} else {
    CONFIG += c++11
}
```
Qt6要求C++17。这里表示编译器支持该标准，不是要求现在把全部代码改写成复杂C++17语法。

## 改动二 媒体API
|功能|Qt5|Qt6|
|---|---|---|
|设置文件|setMedia|setSource|
|状态信号|stateChanged|playbackStateChanged|
|状态类型|State|PlaybackState|
|错误信号|error的重载信号|errorOccurred|
|读取状态|state|playbackState|
|测试视频帧|QVideoProbe|QVideoSink|
|音频输出|原后端行为|显式QAudioOutput和setAudioOutput|
条件编译写法：
```cpp
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    media->setSource(url);
#else
    media->setMedia(url);
#endif
```
这是编译时选择代码，Qt5编译器不会编译Qt6分支，Qt6也不编译Qt5分支。
QAudioOutput由this作为父对象管理，播放器setAudioOutput接入它。FFmpeg预览仍静音，没有因此实现FFmpeg音画同步。

## 改动三 资源别名
新MinGW生成的Makefile遇到中文图片路径依赖失败。图片内容不变，磁盘名称改成英文，例如camera_off.png。
```xml
<file alias="img/摄像头_关闭.png">img/camera_off.png</file>
```
代码仍可写:/img/摄像头_关闭.png，因为alias保留运行时资源路径。编译器只需要处理英文磁盘路径。
实际9个图片的对应关系可查看resources.qrc，本次没有替换图片设计。

## 改动四 MySQL驱动
Qt6安装没有qsqlmysql.dll，因此仅编译成功还不够。必须构建匹配Qt6的插件。
应用通过QMYSQL驱动，再由驱动调用64位libmysql.dll连接MySQL。
```text
Qt6应用 → Qt6版qsqlmysql.dll → 64位libmysql.dll → 本机MySQL
```
原Qt5的32位驱动和libmysql不能直接拿来混用。数据库服务器协议相同，不代表客户端二进制接口相同。
本次使用本机Qt6.11.2源码和MySQL8.0 SDK，没有下载未知驱动ZIP。驱动放在应用bin-qt6/sqldrivers，不改Qt安装目录。
MySQL SDK原路径含空格，windres预处理失败；复制头文件和导入库到x/mysql-sdk无空格路径后构建通过。临时SDK和构建目录用完清理。

## 驱动怎样构建 可复现步骤
1. 建临时目录，复制MySQL安装目录的include和lib/libmysql.lib到无空格路径。
2. 设置PATH包含Qt6匹配的MinGW和Qt6 bin。
3. CMake源目录用Qt6.11.2/Src/qtbase/src/plugins/sqldrivers。
4. 指定Ninja、gcc/g++、Qt6前缀、MySQL_INCLUDE_DIR和MySQL_LIBRARY，Release构建。
5. cmake --build 构建QMYSQLDriverPlugin目标。
6. 将生成的plugins/sqldrivers/qsqlmysql.dll复制到应用同名子目录。
7. 将MySQL安装的64位libmysql.dll复制到应用目录。
不需要重新编译整个Qt。若换到MSVC Kit，必须重新评估驱动编译和依赖，不能将本次MinGW插件当成通用插件。

## 改动五 部署
用目标Qt6的windeployqt对bin-qt6/class15.exe执行Release部署，收集Qt DLL、平台和媒体插件。
再补入自己构建的MySQL插件与libmysql.dll。windeployqt不能凭空生成缺失的数据库驱动。
本次运行目录保留供实际使用，不是一次性工具。

## 你在Qt Creator怎么操作
1. 打开原项目class15.pro，不要打开某个临时Makefile。
2. 进入项目配置，选择Desktop Qt6.11.2 MinGW64位对应Kit。
3. 如果看不到Kit，在工具 设置 Kits中检查Qt版本的qmake路径和MinGW编译器路径。
4. 构建目录必须使用新的目录，不能复用Qt5的build目录。目录建议使用英文和无空格路径。
5. 选择Release，运行qmake，再构建。
6. 项目将输出到原项目bin-qt6。已经放好的MySQL驱动和运行库可直接复用；换机器或删除输出后必须按文档重新部署。
Qt6启动入口在x/启动NebulaVision Qt6.cmd，Qt5入口仍保留。入口不含数据库密码。

## 面试追问与答案
1. 为什么不是换Qt Creator就算迁移？编辑器与构建Kit不同，最终二进制取决于Qt库、编译器、架构和配置。
2. ABI是什么？编译后函数调用、符号、对象布局等约定；源码看起来相同不代表DLL可以互换。
追问 MinGW和MSVC的Qt插件能混用吗？不能作为兼容方案，应匹配目标Qt构建和工具链。
3. 为什么Qt5和Qt6分输出目录？防止32位/64位、插件及旧二进制相互覆盖，便于比较和回退。
4. 条件编译和运行时判断有什么区别？条件编译在编译时排除不适用API，运行时if仍要求分支代码能通过编译。
5. 为什么QMYSQL driver not loaded不一定是密码错？插件不存在、架构不符或依赖DLL缺失都可能在认证之前失败。
6. 数据库服务器要升级吗？本次不需要，换的是客户端Qt、插件和客户端库，服务器仍是本机MySQL8.0。
7. 为什么构建成功还要实际连接？链接只证明部分二进制依赖可解析，插件动态加载和认证要运行才能验证。
8. 为什么Qt6测试不用QVideoProbe？Qt6媒体帧观察接口改为QVideoSink，测试也必须随API更新。
9. C++17意味着必须用高级写法吗？不是，仍可以使用易读的普通类、明确类型和逐步判断。
10. 如何回退？源码改动已保存在Git，原Qt5程序保留；不要直接把两套DLL混进同一目录。

## 官方资料
Qt媒体接口：https://doc.qt.io/qt-6/qmediaplayer.html
Qt SQL驱动构建：https://doc.qt.io/qt-6/sql-driver.html

## 验证记录
具体编译、数据库连接和各阶段回归退出码在本次迁移验收记录中汇总。不能把Qt6一次构建等同于全部跨平台验收；Linux、真实摄像头及长期压力测试仍未完成。

## 本次实际验收结果
Qt6.11.2/MinGW13.1/64位Release主程序编译通过；同版本MySQL驱动编译通过；运行新库诊断退出码0。
Qt6回归阶段2事件页面、阶段4异步查询、阶段5Qt媒体MP4真实帧、阶段6FFmpeg播放、阶段7本机RTSP断流恢复、阶段8四画面，退出码全部0。
Qt5.9.8/MinGW5.3/32位兼容分支重新完整编译通过；本轮没有重新执行全部Qt5运行测试，之前的Qt5验收记录保留。
阶段3分页SQL没有为迁移修改，日期查询由阶段4回归覆盖；本轮未重复创建阶段3的25条临时测试数据。
Linux、Debug Kit、MSVC Kit、真实摄像头、长期压力测试仍未完成。默认播放器仍是FFmpeg进程适配，Qt媒体后端作为可编译且已测试的替代实现保留。
补充部署实测：清除Qt和MySQL开发路径后，首次诊断失败。使用objdump检查发现libmysql.dll还依赖libssl-3-x64.dll和libcrypto-3-x64.dll，已从同一MySQL安装复制到bin-qt6并重新验证。这说明数据库插件之外的传递依赖也必须部署，不能只复制qsqlmysql.dll。
最终独立部署测试通过：PATH仅包含Windows系统目录，Qt6新库诊断退出码0。
