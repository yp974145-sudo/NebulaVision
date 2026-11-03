# 阶段14 Redis状态和限流学习手册

## 结果
摄像头心跳用带TTL的Redis键，在线状态不再把数据库静态status当实时事实。Redis缺失时返回unknown和unavailable，避免把缓存故障伪装成全部离线。
接口按用户每分钟120次，Lua原子INCR并只在首次EXPIRE，超额429。Redis不可用降级为进程内计数，仍保留认证和数据库权限。

## 技术和文件
backend/redisclient.h/.cpp实现本轮所需RESP命令、回复解析与超时；apiserver增加status、heartbeat和摄像头状态；monitorwidget显示设备状态。
本机无Docker/WSL，运行redis-windows项目的Windows移植包，仅绑定127.0.0.1:16379，不安装系统服务，不当作官方Linux生产部署。
本轮原生Redis键和Lua命令已在该移植版实测，Linux原版和生产拓扑未验证。

## 数据流
管理员heartbeat → 验证camera存在 → SET nv:camera:id online EX ttl。
列表 → GET键：存在online，不存在offline，连接失败unknown。
受保护API → 验证身份 → EVAL计数 → 超过阈值429 → 正常业务查询。
Redis故障时心跳写入503，读取状态unknown；事件查询可继续，不降低原有权限要求。
状态本身不能取代设备注册和启用配置，MySQL仍是持久配置来源。

## 实测
redis_check通过：实际Redis进程PING、TTL=2心跳上线、2.3秒后离线、请求达到限流429、停止Redis后unknown、本地限流降级、事件200、心跳503、重启恢复。
测试只删除本轮nv:rate用户分钟键，不执行FLUSHDB。Redis测试进程结束，后续由本地启动器管理运行实例。

## 限制
本地限流不能跨进程共享，不能宣传Redis宕机后仍有严格全局配额。
当前每次命令新建连接，未做连接池和集群；RESP解析仅支持本轮需要的简单、整数和bulk回复，不是完整通用Redis客户端。
登录失败计数仍有进程内保护，业务请求使用Redis；后续风控需按IP、账号等综合设计。

## 入门逐段讲解
### 心跳就是定期续期
例如键`nv:camera:1`值online，TTL为6秒。每2秒续一次，设备代理停止续期后键消失。数据库的设备配置不跟着删除。
### 三个状态必须分开
online是最近有心跳；offline是Redis可用但键已过期；unknown是Redis不可用，根本不知道键是否存在。把unknown显示成offline会误导用户。
### 计数为什么写Lua
```lua
local n = redis.call('INCR', KEYS[1])
if n == 1 then
    redis.call('EXPIRE', KEYS[1], ARGV[1])
end
return n
```
local n是Lua里的局部变量；KEYS[1]是传入的键，ARGV[1]是过期秒数。Lua下标从1开始，不要与C++数组从0开始混淆。
Redis一次执行完整脚本，避免INCR成功但EXPIRE还没执行时客户端崩溃，使计数永远不过期。
### RESP是什么
是Redis客户端与服务端之间的传输格式。本例只实现所用命令和简单回复，不是完整驱动。QTcpSocket在工作线程里连接、发送和等待，有短超时；不能拿去GUI线程大量同步调用。
追问：降级本地计数能等于Redis全局计数吗？答案：不能，多进程各自记账，只是保底，不保证严格全局额度。

## 面试追问与答案
1. TTL为什么适合心跳？持续续期表示最近仍有活跃信号，停止续期自动过期，不必数据库高频写状态。
2. Redis掉线为什么不能当设备掉线？不知道缓存内容与不存在键是不同情况，必须有unknown状态。
3. INCR和EXPIRE为何放Lua？两条单独命令之间进程可能崩溃，导致键无过期；Lua在Redis端原子执行这组逻辑。
4. Redis故障怎样限流？进程内保底，但明确失去多实例全局一致性。
5. Redis能替代MySQL吗？本项目不这样用，配置/权限/录像索引仍在MySQL，Redis保存可过期状态。
6. 120次/分钟是滑动窗口吗？当前固定分钟窗口，边界可突发，生产可改令牌桶或滑动窗口。

## 资料
https://redis.io/docs/latest/operate/oss_and_stack/install/archive/install-redis/install-redis-on-windows/
https://github.com/redis-windows/redis-windows

## 练习
观察同一设备online、offline、unknown的区别，分别对应活跃、过期、依赖故障三种事实。
