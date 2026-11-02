# NebulaVision（星云智视）源码审计与改造技术设计

版本：0.9（总档案原文待补齐）｜审计日期：2026-11-01

## 1. 阅读范围与证据边界

本次只读取 C:\Users\23974\Desktop\jk\class15，不修改、移动、重构原项目，不安装依赖、不执行构建、不启动已有 exe、不连接数据库。本文输出至独立工作目录。所有“已实现”仅表示存在相应源码，不表示已编译或运行验收。

读取了项目 .pro、main.cpp、model/service/util/view 下 C++ 文件，列举资源及 bin。此前聊天 6ae61145-c494-83ec-8416-baa5aeb8573d 已读取，接口返回无更早分页；能够恢复的确定方向是 Qt、跨平台、CMake、C++ 后端、音视频、MySQL、Redis、Drogon、gRPC、ZLMediaKit、FFmpeg、YOLO、Agent、Git/CI/CD、故障恢复与面试准备。具体边界和接口属于本文设计建议，不能声称全部已在旧讨论确认。

**总档案缺失**：本次聊天未带附件；对本地 Desktop、Downloads、Documents 的 NebulaVision* 文件搜索未返回文件；引用聊天 attachments 为空，只有原 ChatGPT sandbox 下载链接。未实际读取 NebulaVision_项目总档案.md，不能宣称完成与17章原档逐项核对。收到原文件后应补充逐条追踪表、冲突记录并升至1.0。本文不包含大创内容。

状态定义：已实现＝源码证据；待验证＝运行、环境、数据或外部文件证据不足；规划＝未来设计。不存在性能实测结果。

## 2. 现状证据与差距矩阵

以下路径均相对原项目根目录，行号按本次读取快照。

|能力|状态|证据及边界|
|---|---|---|
|Qt Widgets 桌面应用|已实现|class15.pro:7–25，Core/Gui/Sql/Widgets，C++11；不是CMake工程|
|主界面及导航|已实现|main.cpp:9直接显示mainWidget；view/mainwidget.cpp的堆叠布局切监控与查询|
|单/四画面布局|已实现|view/monitorwidget.cpp:74–126，QLabel显示关闭摄像头图片；只切布局|
|设备接入|规划|monitorwidget.cpp的设备列表为四个硬编码字符串，无摄像头发现、配置或拉流|
|登录|已实现，运行待验证|usermodel.cpp:14–19，绑定参数查询admin_user，校验status=1；userservice.cpp串联更新登录时间|
|验证码|已实现|verifycodelabel.cpp生成绘制四字符；loginwidget.cpp本地比较；不能充当服务端防暴力措施|
|注册|仅界面已实现|registerwidget.cpp:37–41仅取消按钮连接，未见注册写库流程|
|录像与事件查询|已实现，数据待验证|recordmodel.cpp:14–45，video/exception_log左连接camera_channel、日期过滤、稳定排序、LIMIT/OFFSET|
|分页参数限制|已实现|recordservice.h:8–9，校验日期、页码下限、每页1至100|
|回放|部分实现|reviewwidget.cpp:74–81检查本机文件后交系统默认程序打开；不是内嵌播放器，也不是服务端录像访问|
|数据库连接|已实现，环境待验证|dbconn.cpp:39–47，QMYSQL、本机smart_monitor，硬编码高权限账号与口令；本文不复述口令|
|视频解码/录像/AI|规划|构建及源码未见FFmpeg、Qt Multimedia、推理或录制实现|
|C++服务/API/Redis/gRPC|规划|未见相关源码及依赖|
|Windows可运行|待验证|bin有exe、MySQL DLL及多个Qt版本驱动zip；文件存在不能证明可运行、源码对应或ABI一致|
|Linux支持|待验证|主要使用Qt接口，但无Linux构建和运行记录；pro的unix安装规则不是跨平台验收|
|Git|待验证|根目录无.git；git status被环境拒绝，未确认祖先目录是否为仓库，不能判断未提交修改|
|测试、CI、部署脚本|规划|本次文件清单未见测试目录、CMakeLists、工作流、Dockerfile或SQL迁移文件|

可复用：窗口布局、资源/QSS、日期选择器、分页DTO、Service/Model初步职责分离、参数化SQL、稳定排序、关闭前释放窗口与连接。保留这些价值，逐步替换基础设施，不复制两套长期演进的业务。

## 3. 优先风险与处理建议

1. **鉴权缺口，P0**：main.cpp直接打开主窗口，MainWidget查询按钮直接调用showReview；不存在统一会话权限门禁。后续需服务端对每个资源鉴权，客户端门禁只改善体验。验收：匿名请求录像、事件、播放均被拒绝。
2. **凭据与密码，P0**：dbconn.cpp硬编码数据库高权限凭据；密码使用无盐MD5。迁移为服务端最小权限账号及外部秘密，密码采用成熟Argon2id实现，旧哈希登录成功后升级或强制重置。不要将MD5称作加密。
3. **UI阻塞，P1**：reviewwidget.cpp:93同步Service查询，登录同样同步SQL；慢连接会阻塞事件循环。过渡期专用数据库工作线程独立连接，最终切异步API；不能把现有默认连接直接交给任意线程。
4. **登录密码显示，P1**：loginwidget.cpp创建ediy_pwd后未见Password回显设置；注册页有该设置。需实际界面验证后修复。
5. **本地路径信任，P1**：查询返回数据库任意路径，再交外部程序打开。规划改为按录像ID授权获取受限媒体URL，服务端验证存储根目录与规范化路径，避免越权及路径穿越。
6. SQL schema未提供：exception_log使用chennel_id这一拼写，必须先核对真实DDL；不能直接改名。计数和列表为两次查询，并发写入可能不一致；允许弱一致或用事务快照。深页OFFSET成本需执行计划证明后决定游标分页。
7. DbConn/WinManager懒加载单例没有并发保护；当前主要为GUI线程使用，不能直接扩展为多线程全局依赖。记录连接生命周期与销毁顺序后逐步注入依赖。
8. 最小窗口1800×1000与固定字号需适配低分辨率、DPI和Linux字体。设置按钮未见业务连接；登录取消按钮未见连接；需要交互清单补齐。

## 4. 最终目标架构（全部规划）

```text
Qt Widgets 客户端 ──HTTPS/REST── C++业务服务(Drogon)
      │                           ├─ MySQL：权威业务数据
      │                           ├─ Redis：短期缓存/限流（后续引入）
      │                           ├─ 流管理适配器 ─ ZLMediaKit ← RTSP摄像头
      └─ 授权媒体会话 ─────────────┘          ├─ 分段录像存储
                                              └─ AI采样/推理工作进程
C++业务服务 ← 内部RPC/任务结果 ← 推理服务 → 事件规则 → 告警
Agent服务 ─ 只读工具API → 事件/录像/统计 → 带证据报告
```

控制平面管理账号、通道、策略、元数据和任务；媒体平面处理码流，避免视频字节穿过业务API。初期同机多进程与单体业务服务，后期按实际瓶颈拆分。分布式意味着明确网络边界、超时、重试、幂等和故障处理，不能仅凭用了多个组件宣称完成。

客户端不直连中心数据库，不持摄像头管理凭据。摄像头凭据由服务端加密保存，密钥独立管理。录像文件位于持久化存储，MySQL只存索引及状态。Redis不是权威源，失效时数据库业务仍应可用。

## 5. Qt分层与并发

保留view，逐步形成Presentation（Widgets/Presenter）、Application（用例和会话）、Domain（Camera/Event/Recording及规则）、Infrastructure（HTTP、播放器、配置、日志）四层。View发出意图并渲染DTO，不写SQL、不控制推理、不拼流媒体管理请求。

映射：LoginWidget→LoginUseCase→IAuthGateway；ReviewWidget→QueryRecordingsUseCase→IRecordRepository；MonitorWidget→CameraPresenter→IStreamSession及IVideoPlayer。原UserModel/RecordModel先作为LegacySqlAdapter封装，过渡期两种实现通过接口选择，完成服务端迁移后退出客户端发布包。

网络使用异步QNetworkAccessManager，统一处理超时、取消、401刷新和错误映射。QWidget只能在GUI线程更新；解码/采样工作线程通过队列信号传不可变帧或共享只读缓冲，不传短生命周期裸指针。播放器有Idle/Connecting/Playing/Reconnecting/Stopped/Error状态；关闭窗口先取消任务、停止线程、释放帧和解码上下文。每通道有界队列，实时预览拥塞丢旧帧，录像任务不沿用同一丢帧策略。切通道使用generation/request ID丢弃旧响应。

列表较大后用QAbstractTableModel替代逐格QTableWidget；保留服务端分页，不为样式重写业务。Qt SQL连接的线程归属需遵守官方约束，参见[Qt线程支持](https://doc.qt.io/qt-6.10/threads-modules.html)。

## 6. 技术决策与替代

以下为候选基线，版本、许可及兼容性必须用锁定版本试验确认。

|需求|建议|替代与选择代价|
|---|---|---|
|桌面界面|保留Qt Widgets|QML适合动画和触控，但迁移成本高；当前不值得重写|
|语言/构建|新模块C++17、CMake targets/Presets|保留qmake作迁移对照；C++20需工具链验证，暂不强制|
|业务HTTP|Drogon候选|Boost.Asio/Beast控制更细但开发量大；Qt HTTP适合轻量验证；先做登录、鉴权、数据库垂直样例|
|媒体接入与分发|ZLMediaKit|直接FFmpeg可做单路原型但需自建会话与分发；GStreamer插件链强但部署复杂|
|解码播放|FFmpeg封装候选|Qt Multimedia先做协议样例，省开发但平台后端行为需验证；不要假定均支持同一RTSP路径|
|关系数据|MySQL|SQLite适合本机演示，不能代表中心并发；PostgreSQL可替代但迁移学习增加|
|缓存/限流|Redis按需引入|初期进程内缓存简单；多副本一致限流或共享状态出现后再部署Redis|
|内部RPC|gRPC用于稳定推理契约|初期HTTP JSON更易调试；禁止为了技术名词把所有内部调用都RPC化|
|目标检测|YOLO候选、独立推理进程|CPU ONNX Runtime跨平台路线，GPU TensorRT为后续优化；模型导出、精度、许可证需单独审查|
|Agent|工具调用+结构化输出|先实现确定性统计报表作为基线；不需要先建向量库，不能让LLM执行任意SQL或shell|
|部署|Linux服务Docker Compose，Windows原生Qt客户端|Kubernetes仅有实际多节点运维需求才考虑|

ZLMediaKit提供API与Hook，具体字段和鉴权行为必须针对锁定提交核对；参见[官方指南](https://docs.zlmediakit.com/guide/)和[Hook文档](https://github.com/zlmediakit/ZLMediaKit/wiki/MediaServer支持的HTTP-HOOK-API)。本文不把候选接口当成现有适配代码。

## 7. 数据库设计与迁移

现有SQL引用admin_user(id,username,password,status,last_login_time)、camera_channel(id,camera_name)、video(id,video_name,channel_id,start_time,end_time,file_size,video_path)、exception_log(id,chennel_id,event_time,event_desc,related_video_path)。这些是源码引用字段，未核对真实类型、外键、索引、唯一约束和表是否存在。

目标表：users、roles、user_roles、cameras、recordings、events、alert_deliveries、ai_jobs、agent_reports、audit_logs。users有唯一username、password_hash、status；cameras有组织/所有者、stream_key、加密凭据引用、状态；recordings有camera_id、UTC起止时间、object_key、bytes、checksum、writing/ready/failed/deleted；events有camera_id、type、occurred_at、confidence、model_version、rule_version、evidence_recording_id；任务有幂等键、尝试次数、租约、错误码。

录像索引(camera_id,start_time,id)，事件索引(camera_id,occurred_at,id)，任务索引(status,next_retry_at)；实际以EXPLAIN和数据规模确认。统一UTC存储，UI按用户时区展示；日期查询先转换为UTC半开区间，避免夏令时和跨午夜遗漏。大小统一字节，接口明确单位。

迁移按编号SQL执行并记录schema_version；先备份、校验行数和抽样，再迁移。旧chennel_id可先通过映射兼容，不直接破坏查询。旧文件路径转换为object_key时校验文件存在、根目录、大小；缺失文件标记unavailable。数据库提交与文件落盘不是同一事务：临时文件写完校验后原子重命名，再更新ready；定时对账清理孤儿与残留writing。

## 8. 服务接口草案

统一/v1、JSON、request_id、UTC ISO8601；分页size限制1–100。错误格式为code/message/request_id，内部SQL错误只写脱敏日志。400参数错误，401未认证，403无权限，404不存在或按策略隐藏资源，409冲突，429限流，503依赖不可用。

|方法与路径|输入/输出及权限|
|---|---|
|POST /v1/auth/login|username/password；返回短期访问凭证与刷新机制；限流，统一失败提示|
|POST /v1/auth/refresh、logout|轮换/撤销刷新凭证，服务端记录会话|
|GET/POST /v1/cameras|按权限列举/管理员创建；列表不返回摄像头密码|
|POST /v1/cameras/{id}/stream-sessions|返回短期授权播放URL、expires_at、session_id；校验观看权限|
|DELETE /v1/stream-sessions/{id}|幂等关闭会话；无消费者后按策略释放拉流|
|GET /v1/recordings、events|camera_id/from/to/cursor/size；返回items/next_cursor，必须资源级鉴权|
|POST /v1/recordings/{id}/playback-sessions|短期受限回放URL；支持播放器所需范围访问策略|
|POST /v1/events/{id}/ack|操作员确认，version防止并发覆盖，记录审计|
|POST /v1/agent/reports|时间范围/通道/问题；返回job_id，异步查询报告状态|
|GET /health/live、/health/ready|进程存活/依赖就绪分开；对外不暴露秘密|

内部推理请求至少有job_id、camera_id、frame_ts、受控帧引用、model_version、deadline；结果含检测框、类别、置信度、采样时间和错误状态。Hook验签或受控网络鉴权，校验重放时间窗口，事件ID唯一去重；不可只信来源IP。

## 9. 业务全流程与异常分支

1. 管理员添加摄像头→校验地址白名单和权限→服务端保存配置→测试拉流→报告在线/离线及原因。禁止任意URL访问内网敏感地址，防SSRF。
2. 用户登录→申请通道会话→业务服务授权→媒体节点拉流/复用→播放器连接→展示首帧与状态。超时取消，指数退避加抖动，用户停止后不得继续重连。
3. 录像策略触发→切片写临时文件→封片校验→索引ready→容量策略保留/清理。磁盘不足先告警并停止或回收合规旧片，不能静默生成成功索引。
4. AI工作进程按配置采样→推理→规则引擎聚合连续命中、区域及冷却→写事件和证据关联→投递告警。推理失败不应中断预览和基础录像。
5. 查询日期/通道→授权查询索引→分页显示→选择事件定位录像时间→获取受限会话→播放/定位。缺片、过期、无权限显示不同业务状态。
6. 告警使用事务outbox或持久化待投递记录，与事件一起提交；消费者幂等、重试封顶并有失败队列/人工重发，避免通知失败丢事件。
7. Agent读取授权范围的事件统计和证据→调用白名单只读工具→生成摘要、引用事件ID/时间/模型版本→结构化校验→保存报告及工具调用记录。数据不足明确说明；不把目标检测类别推断成身份，不自动删除录像或执行设备控制。

## 10. Windows/Linux构建与部署方案

当前仅qmake工程，Qt具体版本、编译器和数据库服务未验证。bin中的多个版本驱动不能混用，需Qt版本、编译器ABI、架构匹配。不得将旧exe视作本次源码构建成果。

后续先备份并建立基线，再在独立构建目录复现qmake；记录Qt/编译器/MySQL客户端库/插件版本、驱动发现日志、干净机器运行情况。原pro不删除，CMake迁移对照其全部source/header/qrc。新增目标nebula_client链接Qt6::Widgets、Core、Gui，迁移期间加Sql，之后去掉Sql并加Network；启用AUTOMOC/AUTORCC、target级包含路径，禁止写死开发机路径。

规划目录：apps/client、services/api、services/inference、libs/domain、libs/contracts、adapters/media、tests、deploy、docs。先形成模块边界，再渐进移动文件；不是本轮要执行的目录重排。

建议统一CMakePresets.json提供windows-msvc-debug/release和linux-gcc-debug/release，用户安装路径放不提交的CMakeUserPresets.json。示意命令仅在预设建立后有效：

```text
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug --output-on-failure
```

Linux同理用linux-gcc-debug。先选择一组实际可获得、授权合适的Qt6版本锁定，Windows MSVC或现有MinGW二选一建立稳定基线，不混链接二进制。Linux用GCC构建并在真实桌面环境测字体、插件、音视频、硬件解码；无头单测不能替代桌面验收。

Windows发布使用匹配工具链的部署工具收集Qt插件与运行库，检查platforms/qwindows、图像插件及媒体依赖；干净机器验证不依赖Qt Creator PATH。Linux原生客户端按目标发行版打包并验证插件路径与RPATH。服务采用多阶段镜像、非root、健康检查、持久卷；Compose管理API/MySQL/媒体/推理及可选Redis。限制公开端口，数据库及媒体管理API仅内部可达，TLS在受控网关终止。GPU容器单独维护CPU降级路径。

部署流程：配置校验→备份→兼容性迁移→启动服务→ready检查→登录/播放/录像/事件冒烟→切流；失败回滚镜像，数据库采用向前兼容扩展迁移，不能盲目回退破坏数据。记录镜像摘要、schema版本、配置版本与许可证清单。

## 11. Git、CI/CD与协作

先核实仓库位置，不自动git init或提交。形成原始源码哈希清单和可恢复备份；排除.pro.user、构建目录、exe/DLL、秘密、录像和模型大文件。已有二进制需确认用途后再决定版本化策略，不在本轮删除。

每个任务单独分支，小提交包含动机、变更、验证及限制；PR检查源码与文档状态一致。依赖锁版本/提交摘要，记录许可证和校验值。CONTEXT保存目标与边界，DECISIONS保存决策理由，PROGRESS只记录实际完成及证据。

CI规划：Windows与Linux矩阵配置/构建→单测→API与临时MySQL集成测试→格式/静态分析→Linux ASan/UBSan；媒体测试用可复现样例源。不能把有摄像头/GPU/秘密的测试强制放公共PR。发布标签才打包、生成校验和与依赖清单；CD先预发布冒烟，再受控发布及回滚。流水线凭据最小权限，第三方action固定可信版本；本轮未创建工作流、推送或发布。

## 12. 可靠性、安全与可观测性

连接、读帧、查询、RPC与Agent均有截止时间；重试只用于可幂等操作。资源有上限：每用户会话数、通道解码线程、帧队列、数据库连接池、推理队列及报告并发。录像优先保证存储一致性，预览优先新鲜度，AI允许降采样，业务API不等待模型长任务。

故障矩阵：摄像头断网→离线+退避重连；媒体进程退出→监督重启与会话重建；数据库不可用→禁止假成功并保留可重放任务；Redis故障→降级或明确限流策略；GPU不可用→CPU/暂停AI并告警；磁盘满→容量告警和保留策略；Agent提供商失败→保留任务状态及确定性统计结果。恢复后用幂等键和对账避免重复事件。

日志统一request_id/camera_id/job_id、组件、耗时、错误码，不记录密码/令牌/完整带凭据URL。指标包含首帧延迟、重连次数、解码丢帧、录像失败、磁盘占用、队列深度、API p95、数据库耗时和推理耗时。时间同步与单调时钟分别用于证据时间和耗时。

服务端RBAC至少管理员、操作员、只读；摄像头级授权贯穿查询、流、证据和Agent工具。数据库参数绑定不能替代授权。使用TLS、秘密轮换、导出审计、备份恢复演练、保留期限和删除权限。Agent把事件描述及外部文本当不可信数据，拒绝其中工具指令；限制工具参数、输出大小、调用次数、预算及数据出域。

## 13. 测试与验收设计（未执行）

|层级|重点用例|通过证据|
|---|---|---|
|现有逻辑|空账号、禁用账号、验证码失败、数据库失败、日期无效、0条/末页/同时间记录|测试输入、预期与实际、日志|
|权限|匿名/过期令牌、跨通道ID、普通用户管理操作、报告越权|接口响应及审计，所有受限接口拒绝越权|
|数据|跨午夜、时区转换、并发写入分页、缺失录像、重复Hook|无重复持久事件；缺片状态可解释|
|媒体|H264/H265样例、1/4路、断网、停止重开、窗口关闭|状态转换与资源释放记录|
|故障|服务重启、数据库断连、磁盘满、推理超时|无静默成功、恢复可追踪、录像索引可对账|
|跨平台|干净Windows/Linux安装、DPI、插件缺失|版本清单、构建日志、安装包及截图|
|Agent|无事件、证据不足、提示注入、工具超时、跨权限问题|每个事实能回查证据，越权工具调用被拒绝|

建议初始目标而非承诺：固定硬件与4路1080p/25fps样例，持续运行2小时无崩溃；记录码率、协议、软件/硬件解码、CPU/GPU/内存、首帧和端到端延迟p50/p95；延迟阈值经第一轮基线再确定。每路码率b Mbps每日存储约10.8b GB（十进制、未含开销），4路4Mbps约172.8GB/日，只是容量估算。压测不能用虚构FPS或无硬件条件的数字写入简历。

## 14. 面试与毕设答辩证据链

本节是项目追问提纲，不是已核实的真实公司面经统计。原档案所称面经研究尚需来源、年份、岗位、原题链接和核实日期，不能编造题频或公司归属。

|模块|可能追问|应准备的实证|
|---|---|---|
|Qt|事件循环、信号槽连接类型、QObject线程归属、父子析构、UI阻塞|慢查询演示与异步改造前后记录|
|C++|RAII、移动语义、智能指针、生命周期、数据竞争|播放器释放路径、ASan记录、队列实现|
|并发/网络|背压、死锁、TCP粘包、超时与幂等|有界队列及故障注入结果；HTTP分帧由框架处理但能解释|
|音视频|封装与编码、PTS/DTS、GOP、关键帧、RTSP/RTP、重封装与转码|抓包、时间戳日志、不同GOP首帧对比|
|数据库|联合索引、事务隔离、分页成本、索引与文件一致性|DDL、EXPLAIN、故障对账记录|
|服务端|事件循环与线程池、连接池、认证授权、限流|API契约及并发测试报告|
|AI/Agent|检测精度、误报、工具权限、幻觉与证据|标注样本、阈值试验、失败案例及工具审计|
|工程化|跨平台ABI、CMake target、CI失败定位、回滚|两平台构建产物、流水线记录、回滚演练|

答辩演示按登录→设备→播放→录像→事件→证据→报告→断网恢复展开。简历只写实际实现模块及可复现测量；第三方媒体与模型贡献明确归属，自研贡献聚焦适配、状态机、规则、权限及工程化。

## 15. 分阶段任务与验收

|阶段|具体工作及涉及文件|退出条件|
|---|---|---|
|P0资料/基线|补齐总档案、DDL、依赖版本；核实Git；备份和哈希；新增docs证据|原档逐项映射、无源码损坏、未知项有清单|
|P1安全及现有闭环|loginwidget/mainwidget统一会话门禁；DbConn配置外置；UserService错误分类；Review异步访问|登录及查询用例通过，UI慢查询可响应，匿名不可访问受限数据|
|P2构建迁移|新增CMakeLists/Presets、锁依赖、CI；保留pro对照|Windows/Linux干净构建运行，资源完整，有可追踪日志|
|P3服务化|新增services/api、迁移SQL仓储；客户端Gateway；DDL迁移与角色权限|客户端不含中心DB凭据，登录/查询API及越权测试通过|
|P4视频闭环|Monitor接真实通道、媒体适配器、播放器状态机、录像索引|1路后4路，预览/录像/回看真实可用，断流能恢复|
|P5事件与告警|推理工作进程、规则、outbox、证据关联|真实样例产生事件，重复结果去重，失败可重试，误报可量化|
|P6Agent|白名单工具、异步报告、引用与权限|统计对账正确，报告可回查证据，无数据不编造，注入/越权用例通过|
|P7发布与答辩|部署脚本、备份恢复、监控、负载与面试材料|两平台交付包、复现实验、故障演练和完整证据归档|

每阶段先完成单路/单用例纵向闭环，再扩规模。课程截止时间紧时以P0–P4为可靠核心，P5/P6依质量门槛递进，保留完整目标架构但不能用未完成组件凑成果。不做工期保证；资料补齐与基线复现后按可用时间排周计划。

## 16. 交付、待确认及下一次检查

本次交付为技术审计与规划Markdown，原源码未修改。尚未完成：总档案逐章核对、真实数据库结构检查、构建/运行验证、Git历史状态、具体依赖版本/许可证矩阵、摄像头协议与样例性能、真实面经来源核实。

下一次应先取得NebulaVision_项目总档案.md及脱敏DDL，确认可用Qt工具链、目标Linux发行版、摄像头/测试视频与机器硬件；随后仅做基线复现并保存构建日志。重构、安装、数据库迁移和发布均属于后续任务，不是本文已执行结果。

附：建议后续维护证据表字段为“需求ID、原档章节、状态、源码位置、验证命令/输入、实际结果、日志/截图、负责人、日期”。文档与源码冲突时保留冲突记录，禁止静默把规划改写成既成事实。

## 18. 总档案补齐后的逐章核对（2026-11-01）

已实际读取 C:\Users\23974\Desktop\NebulaVision_项目总档案.md（v1.0，2026-10-31）。此前“总档案缺失”是首轮审计时的历史状态，现已解除；构建、数据库、性能等未验证状态不变。本文补订版为v1.0。原总档案与原项目均未修改。

|原档章节|本报告对应|核对结果|
|---|---|---|
|01目标约束|1、14、15|补充：2028届实习，2027年底考研优先；限制项目规模与时间投入|
|02岗位简历|14|尚未研究实时岗位；参考简历及8000+连接未经核实，不可借用为成果|
|03源码审计|1–3|静态审计已做；运行基线、许可、性能仍待验证|
|04需求验收|8、9、13、15|原档最终MVP明确含一个YOLO场景、一次Agent工具查询、Compose；之前所称第一版仅为中间里程碑|
|05推演ADR|4、6|候选技术仍需验证；Redis/gRPC按需要引入|
|06架构链路|4、8、9|补充WebSocket事件推送，需鉴权、断线补查与事件去重|
|07 Qt|5|Widgets与现有源码匹配；Model/View、播放器适配及线程边界保留|
|08 C++后端|5、8、12、14|选择1–2个模块深做；建议播放器生命周期/有界队列及重连状态机，不同时自研多个网络框架|
|09视频AI数据|6–9|补充原始检测detections与聚合事件/告警分层；LangGraph与RAG是条件采用|
|10技术选型|6|Drogon为主API候选；vcpkg/Nginx/指标平台/MCP均需实际需求|
|11跨平台|10|目标明确为Windows11+MSVC与Ubuntu+GCC客户端；现有MinGW可用于旧基线，不替代最终MSVC验收；后端Linux，Windows用WSL2开发|
|12 Git|11|尚未确认仓库及历史，不能称已建立工作流|
|13构建CI|10、11|需两平台实际产物，CI绿不替代运行验证|
|14质量|12、13|性能均待测，故障恢复与备份演练进入验收|
|15面经|14|原档链接只是待核实索引，未逐篇核实；未形成真实题频统计|
|16路线|15|16周为理想示例非交付承诺；按考研安排推进|
|17维护|16、18|源档为设计基线，源码/测试证据决定实现状态；后续同步ADR与进度|

### 修订后的开发与学习顺序

1. 基线：重新编译现有qmake项目、跑登录与查询、取得DDL；学Qt事件循环、SQL、调试及Git。验收是有构建日志和可复现操作。
2. 最短视频链路：Qt单路播放，验证ZLMediaKit接入，同时实现一个最小C++ API；学FFmpeg基本流程、HTTP/JSON、线程生命周期。不要先完成所有本机功能才第一次接服务端。
3. 业务闭环：设备/用户/录像/事件API与权限，客户端逐步退出直连数据库；学MySQL索引事务、异步请求、超时和错误处理。
4. C++深度：选有界帧队列、播放器关闭/取消、断线重连等1–2项，做故障测试和测量；学RAII、智能指针、互斥与条件变量、背压。
5. 智能闭环：一个检测场景→持久事件→录像证据→Agent只读查询；学推理输入输出、误报漏报、工具参数验证及证据引用。该阶段完成后才满足原档智能链路要求。
6. 交付：Windows MSVC与Ubuntu GCC实际验证，Linux Compose与恢复演练。CMake、Git、最小CI应提前在阶段1–2开始，交付阶段再完善打包/CD。

当下用户需提供或确认：Qt Creator实际Kit版本、数据库脱敏DDL、CPU/GPU/内存与测试视频/摄像头、每周可用时间及最近截止日期。无需一次安装全部候选组件；每阶段只增加该链路必需依赖。

## 19. 本机基线实际验证（2026-11-01）

- 已验证：使用E:/qt/5.9.8/mingw53_32与MinGW5.3重新编译链接成功；程序启动存活3秒，随后关闭测试实例。未验证界面交互。
- 已验证：本机MySQL服务运行中；独立只读Qt探测失败，未执行表查询。QMYSQL插件可枚举但加载失败，错误0x000000c1。不能认定账号密码错误或数据库缺表。
- bin内libmysql.dll与libmysql_real.dll的PE机器标记均为0x14C（32位）；尚不能确定插件本身或传递依赖中的具体错误文件。不得使用bin内Qt6 64位驱动包替换Qt5 32位插件。
- 构建产物：build-baseline/output/class15.exe；探测日志：build-baseline/db-probe.log与db-plugin-diagnostic.log。
- 操作偏差：第一次构建因.pro内DESTDIR覆盖命令行参数，更新了原bin/class15.exe；未修改源码，未保存原exe副本，不能声称原二进制未变。随后使用qmake -after修正独立输出位置。
- 下一步：定位QMYSQL插件及其依赖架构/导出符号，先恢复旧环境数据库基线，再推进播放模块；不盲目安装和替换系统依赖。

## 20. 连续执行结果与数据库隔离约束

用户明确要求：新功能另建数据库，不改原库。已创建nebulavision_dev（utf8mb4），仅含schema_migrations和0001初始化记录；初始化成功。脚本位于build-baseline/001_create_nebulavision_dev.sql。未复制原用户、录像和事件；未修改smart_monitor。未来业务代码必须显式使用新库配置；原项目运行脚本仍连接旧库，完整登录会更新旧库时间，因此不得作为隔离改造的启动入口。

数据库故障已定位并绕过：为测试进程PATH增加原项目bin后QMYSQL连接成功；原两份MySQL DLL为配套依赖，不能拆开或替换为Qt6驱动。未改全局PATH或Qt安装目录。

已实际验证原RecordService/RecordModel：录像11条，第一页10条、第二页1条、请求999页被约束到2页；事件3条，请求2/999页均约束到1页；无效日期被拒绝。退出码0，日志build-baseline/record-check.log。原库仅只读检查，登录未执行。

已取得四表脱敏列结构，见build-baseline/schema-check.log；chennel_id确实存在。没有导出账号、密码哈希或私人录像内容。

独立video_probe已编译成功（Qt Multimedia），具备打开本地文件/播放/停止/错误提示；尚未实际验证媒体解码、首帧或RTSP，因此不是已完成视频功能，也未集成原项目。程序位于build-baseline/probe-output/video_probe.exe。它用于评估现有后端，FFmpeg最终路线仍需验证。

后续任务队列：1. 新库业务schema与最小权限开发账号；2. 独立配置加载，拒绝新应用误连smart_monitor；3. 新库测试数据与登录/分页验证；4. 测试视频播放及资源释放；5. CMake与Qt6/MSVC迁移试验；6. 真实播放适配器与最小API；7. AI检测与证据链。实施时逐项保留实际日志，不将待办记为完成。

## 21. 新开发目录及业务schema已建立

2026-11-01：独立工作目录NebulaVision已创建；nebulavision_dev执行0002迁移，现有8张表。roles有3个角色、cameras有1个禁用的合成通道、events有1条合成测试事件，users为0。无原库写入和数据复制。配置模板中的应用账号及库名保护尚未实现，后续任务见NebulaVision/README.md。
