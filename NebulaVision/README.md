# NebulaVision 1.0.0 基础验证版

## 启动

双击 dist/NebulaVision.exe。本机MySQL需运行；程序自动读取config/viewer.local.json，连接独立nebulavision_dev。该文件包含本机随机口令，已由.gitignore排除，请勿分享。不要单独分享整个开发目录。

## 本版能力

- Qt Widgets独立客户端，连接状态和错误提示。
- 新库通道查询、最近100条事件查询，刷新及只读表格。
- 数据库名称实际校验；专用nebulavision_viewer_v1账号仅授予新库SELECT权限。
- 本地视频打开、播放/暂停、停止及错误提示，Qt Multimedia后端。
- 当前1条通道和1条事件均为合成演示数据，无真实检测结果。

## 验证记录

Qt5.9.8/MinGW5.3 32位Release编译成功；windeployqt依赖收集成功；启动存活检查通过。原项目分页逻辑验证通过，见上级build-baseline日志。本版只读账号、自动连接及实际媒体播放另见build测试记录；未完成完整GUI自动化验收。

## 边界

这是基础原型，未达到总档案定义的完整MVP。未实现业务用户登录、API服务、RTSP摄像头、真实录像、YOLO、Agent、Linux/Qt6/CMake。查询同步执行，慢数据库可能阻塞界面，后续改为异步。当前本机配置依赖与数据库访问方式适用于开发验证，不作为最终服务化架构。

原class15源码与smart_monitor不在本轮修改范围；本应用代码固定访问nebulavision_dev。口令通过本地文件保存，正式发布应改为适当的秘密管理。
