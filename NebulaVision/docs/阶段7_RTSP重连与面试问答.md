# 阶段7 RTSP超时重连与面试问答

## 本阶段结果
监控页新增打开RTSP按钮，支持rtsp和rtsps地址。使用TCP传输，后台读取视频，断流后1、2、4、8秒退避重连，上限8秒。收到帧后重置退避。
点击停止取消重连，旧会话已经排队的帧和错误不会覆盖新会话。播放仍为静音预览。

## 修改文件
player/ffmpegplayer.h/.cpp增加网络识别、QTimer重试、会话编号和状态；view/monitorwidget.cpp增加RTSP输入按钮；tests/stage7_check.cpp/.pro增加受控流测试。
本机测试工具在x/media-tools：MediaMTX和FFmpeg，不安装系统服务。测试配置绑定127.0.0.1:18554，关闭其他媒体协议。

## 逐步理解连接流程
open先stop上次会话；QUrl判断协议和地址格式。网络地址只保留在当前对象中，不保存到配置或日志。
工作线程给FFmpeg传入-rtsp_transport tcp和-timeout 3000000（微秒），本地文件仍使用-re；网络流不额外按文件速度限流。
仍有5秒无数据保护，超时会清理解码子进程。finished回到GUI后安排单次重连定时器。
startDecoding只清理旧任务，不把重连开关关闭；stop才关闭开关、增加generation、停止timer并清理线程。

## 为什么用generation
每次新会话都有编号。回调捕获当时的session，收到信号时与generation比较。
即使disconnect前已经排队的信号后来送到，只要属于旧会话就忽略。仅disconnect不能被当作“所有排队回调都已消失”。

## 状态机如何理解
```text
停止 → 正在连接 → 正在播放
                ↘ 失败/断流 → 等待重连 → 正在连接
任何状态 → 用户停止 → 取消重连并清理
```
Qt定时器安排下一次连接，不使用GUI线程sleep等待。
连续失败退避1、2、4、8秒，避免无休止快速创建进程；成功收到帧后回到初始重试间隔。当前没有加入随机抖动，局域网大量客户端同时重连仍可继续改善。

## 实际验证
stage7_check编译通过、退出码0。测试程序启动本机MediaMTX，FFmpeg循环推送合成MP4。
通过：RTSP收到至少5帧；杀掉测试推流进程后触发重连；重新推流后帧数继续增加；停止后帧数和重试次数保持不变；连接未开放端口会触发重试，停止后仍不再重试。
测试自己的推流和服务器进程均在结束时kill并wait，未结束其他服务。
未测试真实摄像头、互联网丢包、rtsps证书、长期运行和摄像头账号认证。rtsps协议识别已加入，但不能据此声称TLS实际验证通过。

## 面试追问和答案
1. TCP一定比UDP好吗？不是。TCP可靠有序但可能产生队头阻塞；UDP延迟表现不同，需要容忍丢包。本阶段用TCP降低本机验证变量。
2. 为什么指数退避？限制失败期间请求频率，减轻设备和网络压力。大规模系统还应增加抖动。
3. 超时和停止是一回事吗？不是。超时是异常检测，可能触发重连；用户停止是明确取消意图，必须关闭重试。
4. 为什么disconnect之后还校验编号？排队信号可能已经投递，会话编号进一步防止旧结果污染。
5. RTSP和视频编码有什么关系？RTSP管理会话，媒体传输通常涉及RTP；H264等是编码，不能把协议与编码混为一谈。
6. 测试用的MediaMTX是业务服务端吗？不是，此阶段只是本机受控测试夹具；后续正式媒体服务按计划设计。
7. 网络地址中的密码会不会显示到日志？应用提示不输出完整URL，FFmpeg错误信息在工作线程排空不透传。输入框目前是普通文本，正式产品应进一步设计凭据交互。

## 官方资料
FFmpeg协议文档：https://ffmpeg.org/ffmpeg-protocols.html
MediaMTX介绍：https://mediamtx.org/docs/kickoff/introduction
MediaMTX FFmpeg推流说明：https://github.com/bluenviron/mediamtx/blob/main/docs/3-publish/17-ffmpeg.md

## 练习
从stop函数找到enabled=false、generation增加、retryTimer.stop，分别解释它们阻止了哪一种旧行为。
