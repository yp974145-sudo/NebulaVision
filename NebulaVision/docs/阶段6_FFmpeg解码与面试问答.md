# 阶段6 FFmpeg单路解码与面试问答

## 本阶段成果和实现选择
监控页单画面默认使用FFmpeg实现，阶段5的Qt实现保留在代码中。
Qt/C++用QProcess启动FFmpeg独立进程，FFmpeg负责读取、解复用、解码、缩放和RGB转换；后台线程读取管道，GUI显示QImage。
这是可运行的FFmpeg进程适配方案，尚未直接集成libavformat/libavcodec，也没有实现avcodec_send_packet/receive_frame循环。面试时不能说这些底层API已经亲自编写。
当前为静音预览，固定640×360、15fps，不是保留原视频音画同步的完整播放器。

## 新增和修改文件
player/ffmpegplayer.h/.cpp新增DecodeTask和FfmpegPlayer；monitorwidget.cpp将实例从QtVideoPlayer换成FfmpegPlayer；接口和按钮操作不变；class15.pro加入实现；tests/stage6_check验证真实帧和生命周期。
工具放在x/media-tools，合成样片放在x/samples，不纳入源码Git。

## 调用链
```text
打开按钮 → FfmpegPlayer::open → 检查文件和工具路径
→ new DecodeTask → start → run在线程中创建QProcess
→ FFmpeg输出RGB24到stdout → 组装完整帧
→ frameReady跨线程 → GUI转QPixmap显示
```
QProcess使用程序路径和QStringList参数，不使用拼接的shell命令。
工具路径依次查环境变量NEBULAVISION_FFMPEG、PATH，再查同级jk/x中的工具。换电脑要重新配置工具，不是源码自带完整安装包。

## 解码循环逐步讲解
停止和暂停标志是std::atomic<bool>，GUI修改，工作线程读取，避免普通bool的跨线程数据竞争。
run内部创建QProcess，所以启动、读取和销毁都在同一线程。
-re让本地文件按媒体速率读取；-an禁用音频；-sn禁用字幕；滤镜保持比例缩放并补黑边；rgb24和rawvideo输出固定尺寸裸帧。
每帧字节数=640×360×3=691200。管道一次读取可能只有半帧，buffer必须凑够一帧才能构造图像。
QImage引用buffer内存，frame.copy使数据独立拥有，后面buffer.clear不会破坏已经送给GUI的图像。
每50毫秒检查输入及停止状态，stderr持续排空，5秒无数据退出。
停止请求后先退出循环，再kill自己的FFmpeg子进程并等待回收。没有杀死其他媒体进程，也没有terminate工作线程。

## 暂停与停止的区别
暂停时不继续读取帧，管道逐步产生背压，恢复后继续处理。它不是通用的精确seek或时间轴暂停。
停止时请求线程退出，等待线程清理进程，再删除线程对象。重复打开前先stop，避免前一条解码继续运行。
阶段8将用有界最新帧缓存，限制GUI积压；本阶段逐帧队列信号还有积压风险，不能声称已经完成多路队列设计。

## 测试结果
Qt5.9.8/MinGW5.3主程序及stage6_check编译通过，测试退出码0。
同一合成H264 MP4在FFmpeg实现实际收到多帧；暂停后帧数稳定，恢复增加；停止后不再增加；重新打开再次收到帧；不存在文件给出错误。
首次重复打开测试等待700毫秒不足，改为1800毫秒后通过，记录的是工具启动时延而非宣称瞬时启动。
没有验证所有编码、损坏文件、音频同步、长时间运行或Linux。

## 面试追问与答案
1. FFmpeg做了什么，你写了什么？FFmpeg进程做媒体解复用解码；我写了Qt接口适配、进程管理、线程停止、帧拼接和显示。
追问 用了send_packet吗？当前没有，直接libav API是后续可替换实现，不能冒充已完成。
2. 包和帧有什么区别？包通常保存压缩编码数据，解码后得到可显示帧；一个包和一帧并不永远一一对应。
3. 为何读取管道不能一次当一帧？管道是字节流，读出的块边界与图像帧边界不一致，需要按固定字节数组装。
4. 为什么copy图像？避免QImage引用的原buffer在清空或重用后失效，这是数据所有权问题。
5. 为什么用原子标志？停止和暂停在不同线程读写，原子提供简单标志的同步；不代表所有复杂状态都可无锁。
6. 停止和取消为什么要等待？确保线程与子进程真正结束再销毁对象，防止后台访问已经释放的资源。
7. PTS/DTS是什么？PTS是显示时间，DTS是解码时间；有重排序时可能不同。当前节奏由FFmpeg处理，应用没有自行实现完整时间同步。
8. 为什么无音频？本阶段目标是监控预览，不做完整音画同步；必须清楚说明能力边界。

## 官方资料
FFmpeg命令文档：https://ffmpeg.org/ffmpeg.html
协议与管道说明：https://ffmpeg.org/ffmpeg-protocols.html

## 练习
计算320×180的RGB24每帧需要多少字节，再指出实际640×360代码里width、height和frameBytes的位置。先不要只改一处尺寸，否则读取帧边界会错。
