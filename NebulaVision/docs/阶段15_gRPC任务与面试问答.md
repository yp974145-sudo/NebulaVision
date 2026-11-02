# 阶段15 独立gRPC检测服务学习手册

## 实现与技术边界
独立Python检测进程提供真实gRPC/Protobuf服务，C++业务服务通过受控本机Python RPC客户端适配器调用它。不是原生C++ gRPC stub实现，不把适配方案说成直接链接grpc++。
选择原因：当前Qt/MinGW工具链没有原生grpc++依赖，Python gRPC可隔离检测运行时；接口由.proto固定，后续可替换C++客户端适配而不改业务契约。
本阶段返回明确synthetic=true的可控结果，只验证通信和可靠性，不产生真实业务告警。阶段16将关闭test模式并接入实际推理。

## 新增文件
 detector/detection.proto定义任务、阈值、ROI及检测框；生成pb2/pb2_grpc代码。
 worker.py校验RPC密钥、输入根目录、幂等记录、并发和截止时间；rpc_client.py接收C++ stdin JSON并调用gRPC。
 backend/detectionjobs.h/.cpp管理异步任务和RPC适配进程；API提供start/job；0004迁移新增detection_jobs。
 Python依赖在x/runtime-python，模型及状态目录属于正式运行资源，密码和密钥在本地配置，不放Git。

## 请求流
管理员提交录像id和jobKey → 服务端验证录像ready及文件存在 → 输入参数指纹 → 数据库唯一job_key → 返回202 → 后台RPC适配进程 → gRPC检测进程 → 回写任务状态和结果。
客户端不能传任意磁盘路径，服务端根据录像索引解析文件；RPC进程再次限制路径只能位于配置根目录。
桥接密钥通过stdin，不出现在命令行和日志。仅本机明文gRPC，跨机器部署需TLS/服务身份另设计。

## 幂等与失败
数据库唯一job_key保证业务任务不重复；相同键但录像/阈值/ROI不同返回409。
检测进程SQLite保存请求指纹和完成响应；重复RPC返回缓存，改变负载拒绝；正在执行的同一任务拒绝并发重复。
最多2个推理并发，业务排队上限8，RPC deadline5秒，适配进程总等待8秒，异常写failed/error_code。
服务重启把残留queued/running标failed，不假装任务已自动恢复。
取消是协作式，worker测试延迟循环检查context；后续模型推理执行中不保证立即抢占CPU。

## 实测
rpc_contract_check通过：RPC缺认证拒绝、首次结果、重复命中缓存、参数冲突、50毫秒deadline、future.cancel。
detection_contract_api_check通过：C++接口至独立gRPC进程完整链路、任务落库、相同键重试、冲突409、viewer拒绝；events数量不增加，证明合成响应没有冒充告警。

## 面试追问与答案
1. RPC和本地调用区别？有网络、序列化、超时和部分失败，不能假设函数调用一样可靠。
2. Protobuf有什么用？明确字段编号和类型，生成双方代码；字段编号应保持兼容，不随意复用删除字段。
3. deadline后服务端一定停了吗？不是，客户端停止等待，服务端还需检查取消；底层推理调用可能执行到返回。
4. 为什么数据库和worker都做幂等？业务任务创建与RPC重试是不同边界，分别防止重复创建和重复执行。
5. exactly once实现了吗？没有。通过唯一键、缓存和状态尽量避免重复副作用，不承诺网络下绝对恰好一次。
6. Python适配器是HTTP伪RPC吗？不是，适配器实际使用grpcio生成stub调用独立gRPC服务，但C++侧仍有额外进程边界。
7. 生成代码要手改吗？不要，修改proto后用相同工具重新生成，生成代码作为运行源码保留。
8. 进程挂了怎么办？业务任务标失败，重启后的中间状态可识别；自动重试/恢复策略尚未实现。

## 练习
找到job_key、input_hash和worker SQLite fingerprint，分别说明它们在哪个边界检查重复。
