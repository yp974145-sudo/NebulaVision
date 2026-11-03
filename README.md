# NebulaVision 项目与学习文档

在原 Qt Widgets 监控程序上逐阶段改造的学习项目。客户端入口是根目录 `main.cpp` 和 `class15.pro`；C++ API 服务在 `backend`；检测服务在 `detector`。

## 文档从这里看

- [报告与学习文档目录](NebulaVision/docs/README.md)：设计报告、阶段讲解、面试问答、迁移和问题修复。
- [Word 文档](NebulaVision/docs/Word)：可下载的阶段 2–16 与 Qt6 迁移学习文档。
- [阶段路线](NebulaVision/docs/沿原源码升级_任务与面试路线.md)：阶段规划，完成情况应结合代码与验收记录核对。

`NebulaVision/README.md`描述的是早期独立基础验证版，不能代替当前原项目的实现说明。

## 当前实现和边界

当前原程序接入 Qt6 HTTP Server API、业务登录与资源权限、MediaMTX 授权媒体、短片录像和回放、Redis 心跳与限流，以及通过本机 Python 适配器调用 gRPC 检测服务。真实 YOLO ONNX 模型对录像抽帧进行人员检测，事件可关联录像证据。

实际采用 Qt HTTP Server、MediaMTX 和 Python gRPC 适配，不能宣称采用 Drogon、ZLMediaKit 或原生 C++ gRPC。当前为本机开发验证：录像是有时长限制的短片，检测是单帧抽样与区域规则，不代表全天连续录像、连续轨迹检测或生产部署完成。

阶段17已加入LangGraph只读查询、checkpoint恢复和证据模板报告；阶段18已加入本机中文嵌入模型与Qdrant文档检索，原Qt窗口完整调用链已验证。尚未接大语言模型生成。阶段19已验证Windows CMake构建与服务管理器两轮启动停止，Linux、MSVC、Compose和干净机器分发仍待验收，当前没有上传GitHub。

## 构建和本机资源

主程序使用 Qt6 MinGW 64 位 Kit 构建根目录 `class15.pro`；API 服务构建 `backend/server.pro`。检测服务依赖、模型来源及配置见 `detector` 中的说明和清单。

仓库保留配置样例、数据库迁移、源码和可复现测试；机器专用路径需要根据实际环境调整。运行需要自行准备本机 MySQL、新库及业务账号、媒体工具、Redis、Python 依赖和模型。当前 Windows 启动管理器 `scripts/local_demo.py`还依赖本机外部资源目录，不能只克隆仓库就直接运行。

数据库密码、应用账号密码、签名密钥及 RPC 密钥保存在被忽略的本地配置或环境变量中，不随仓库提供。配置示例不能代替本机真实配置。原库 `smart_monitor`与新库 `nebulavision_dev`分开保留。
