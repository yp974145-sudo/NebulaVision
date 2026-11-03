# 账号 摄像头与YOLO测试说明

## 本次结果
2026年11月3日沿原程序补上管理员创建账号、本机摄像头预览，并精简登录页。客户端Release、Debug及C++后端编译通过。测试发现本机USB2.0 HD UVC WebCam，并成功收到真实帧，没有保存摄像头画面。

演示视频走完整业务链：授权RTSP解码、录制短片、YOLO真实推理，得到录像4、事件31、区域内2人。这是用含人物的公开图片循环生成的视频样本，不是对本机摄像头进行的实时检测。

## 用户操作
先运行x下启动完整演示.cmd。后台控制台可以一直开着，也可以最小化；关闭通过Qt Creator前台运行的业务后端通常会停止该后端。演示脚本管理的服务在后台运行，不要求保持一个命令窗口显示。停止服务请用停止完整演示.cmd。

管理员创建账号：使用admin登录，点顶部创建账号，填写用户名、两次密码，勾选允许新用户访问的设备，然后创建。用户名为3至32位字母、数字或下划线，密码6至128位。新账号固定为普通只读用户，不能在这里创建管理员。不勾选设备时，新用户的设备列表为空。普通用户进入该页会看到无权限提示，服务端也会拒绝提交。

本机摄像头：监控页点击本机/USB摄像头，选择USB2.0 HD UVC WebCam，打开独立预览窗口。关预览窗口停止采集；全部停止或退出登录也会停止本机采集。没有设备时检查USB、设备管理器与Windows摄像头隐私设置；设备占用和权限错误会显示在预览窗口。

网络摄像头：本机摄像头按钮不用于网络设备。已有RTSP地址时使用打开RTSP；左侧服务端演示设备则要双击设备列表，程序自动申请授权URL。旧的错误默认路径已去掉。手动RTSP只提供当前预览，没有自动建立业务设备、后台录像和权限映射。真实网络摄像头还需要按品牌型号取得地址，并验证网络、账号和编码，不能只填一个IP。

YOLO演示：使用admin登录，在左侧刷新设备并双击演示设备，选择它后点击录制所选设备6秒。等待完成后进入回放，选择录像记录，再点全部记录。选择新产生且状态为ready的录像，点击检测所选录像，等待人员数和事件ID。随后切换异常事件、点全部记录，双击对应事件可回放证据。日期默认可能筛掉新记录，所以首次测试建议选全部记录。

目前YOLO使用YOLOv5n ONNX单帧采样检测人员，不会在实时预览画面持续画框，不是完整行人轨迹或区域入侵检测。USB预览还没有自动推送进媒体服务器，因此不能直接在这个本机预览窗口点后台检测。

## 参考项目摄像头范围核对
BetaVideoMonitorClient：官方README明确使用网络摄像头RTSP主、子码流，也提供HTTP视频地址测试。没有从README确认现成USB采集或ONVIF发现入口。48路是作者列出的播放录像能力，README同时说明当前24人共用两条配置地址及测试限制，不能直接解读为已实测48台独立摄像头。
来源：https://github.com/onlyet/BetaVideoMonitorClient

hms-detection：官方README明确每摄像头RTSP采集，示意结构经go2rtc转流后进入FFmpeg及YOLO。没有从README确认直接选择USB设备的入口；要使用USB需先由采集工具转成其可读取的流。其检测、环形缓冲与事件录制比本项目当前抽帧演示更完整，不能把参考功能写成自己已经实现。
来源：https://github.com/hms-homelab/hms-detection

VideoSurveillance：官方README明确以Tapo网络摄像头的RTSP为主，并给出通用VS_CAMERA条目和TC72、C210、TC70旧配置名。其他品牌能否兼容要实际测试流地址和编码；没有从README确认USB或ONVIF发现。
来源：https://github.com/Bander2603/VideoSurveillance

ZLMediaKit：C++流媒体底层支持RTSP、RTMP、WebRTC和GB28181媒体相关能力等。GB28181媒体接收不等于提供完整SIP设备注册平台；USB通常先由FFmpeg等采集推流。ONVIF设备发现、取流地址和云台控制也不能因支持RTSP就自动视为完成。
来源：https://github.com/ZLMediaKit/ZLMediaKit 及 https://github.com/ZLMediaKit/ZLMediaKit/wiki/GB28181推流

MediaMTX官方给出Windows用FFmpeg dshow采集USB并推RTSP的示例；也说明RTSP网络摄像头接入。ONVIF Profile S/T设备通常提供RTSP，但这不等于本项目实现了ONVIF发现与控制。
来源：https://mediamtx.org/docs/publish/generic-webcams 及 https://github.com/bluenviron/mediamtx/blob/main/docs/3-publish/08-rtsp-cameras-and-servers.md

## 代码改动和原理
loginwidget.cpp去掉本机路径说明和不可点击的账号按钮，标题改为登录，保留输入、验证码、显示密码及返回。

mainwidget.cpp增加创建账号入口，复用原RegisterWidget，未登录先提示登录。registerwidget.cpp重新接到现有API，展示设备多选列表；通过QFutureWatcher在工作线程请求，GUI只显示结果。generation过滤关闭页面后的过期回调，密码输入在提交与关闭时清空。

backend/apiserver.cpp新增POST /api/users。先验证登录和管理员角色，再检查用户名、密码和设备ID。密码沿用PBKDF2-SHA256、20万次迭代、随机16字节盐。事务内一次写users、user_roles和user_camera_permissions，失败回滚；重复用户名返回409。客户端不能指定管理员角色，未登录401、普通用户403。

monitorwidget.cpp使用QMediaDevices枚举本地视频设备，QCamera负责采集，QMediaCaptureSession连接采集与QVideoWidget显示。Qt6条件编译保留原项目结构。本机采集和FFmpeg RTSP播放器是两种输入路径，USB不是RTSP地址。

tests/account_camera_check.py测试真实API并创建随机测试用户，验证新账号能登录且只读；完成后按本次随机用户名与ID精确清理权限和用户，不改已有admin、user。tests/local_camera_probe.cpp可复现枚举与收帧检查，仅输出设备名和收到帧的状态。

## 面试追问与答案
问题1：USB和网络摄像头输入区别？答：USB通过本机采集驱动，网络摄像头通过RTSP等协议。追问：能直接把USB名称填RTSP输入框吗？答：不能，要么用本机采集接口，要么先采集编码推成网络流。

问题2：RTSP和ONVIF一样吗？答：不一样，RTSP侧重媒体会话，ONVIF还涉及设备发现、配置和控制。追问：能播RTSP就支持云台吗？答：不能，云台要单独实现控制接口。

问题3：为什么创建账号必须后端鉴权？答：客户端按钮可以被绕过，真正权限在API入口检查。追问：隐藏按钮够吗？答：不够，所以真实测试还验证普通用户直接POST得到403。

问题4：账号和权限为什么放一个事务？答：避免账号创建了却没有角色，或设备授权只写一半。追问：视频文件和SQL也能同样原子吗？答：不能直接跨文件和数据库做同一事务，需要状态和对账。

问题5：YOLO输出框就等于业务告警吗？答：还需区域、阈值、去重、事件与录像关联。追问：当前单帧测试能证明持续入侵检测可靠吗？答：不能，它只证明这条样本链路和模型推理能工作。

## 未测事项
未取得真实网络摄像头地址，未进行某品牌设备实测；没有ONVIF设备发现、国标SIP注册及USB到后台检测的完整链路。本次本机收帧通过不代表所有USB型号兼容；没有保存个人摄像头测试画面。
