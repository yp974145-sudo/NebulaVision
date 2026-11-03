# 阶段10 客户端接入API

## 结果与文件
新增service/apiclient.h/.cpp，EventService从SQL仓储切到HTTP，EventQueryTask继续在后台调用Service并将结果交回原页面。
ApiSession用互斥锁保存内存令牌，为阶段11登录准备。令牌不写文件。
原SQL仓储代码保留作历史和独立参考，但新事件页面不调用它，也不读取数据库密码配置。

## 数据怎么流动
回看页 → 工作任务 → EventService生成page/size/date参数 → ApiClient发送HTTP → C++服务端查询MySQL → JSON → EventData列表 → GUI填表。
网络使用QNetworkAccessManager的异步请求，在工作线程局部事件循环等待完成，8秒定时器超时abort。GUI线程没有嵌套等待。
这不是每个网络请求都需要线程的通用结论；当前沿用阶段4边界，后续可以改成纯GUI异步回调。
本机允许HTTP，远程API地址必须HTTPS。401提示登录，403提示权限，离线和超时提示服务不可达。

## 实测
api_client_check退出码0：给不存在数据库配置路径仍可查事件；1900日期返回0；未开放端口明确返回错误。
阶段10只迁移新事件链路，旧登录和旧录像在随后阶段迁移，不能说整个客户端此时已经没有旧库代码。

## 入门逐段讲解
### API地址里的问号是什么
`/api/events?page=1&size=10`中问号前是接口，后面是参数，&分隔参数。QUrlQuery负责正确编码，避免自己拼接有特殊字符的值。
### 网络请求和返回对象
```cpp
QNetworkAccessManager manager;
QNetworkRequest request(url);
QNetworkReply *reply = manager.get(request);
```
manager管理请求，request装地址和请求头，reply代表这一次请求的过程及结果。get发出请求后不会在这一行同步把所有数据取完。
finished信号表示结束，readAll读已经收到的字节；QJsonDocument解析这些字节，再按字段装进EventData。网络文本和C++结构体不是同一东西。
### 为什么这里还有QEventLoop
本实现沿用阶段4的工作线程，在工作线程里等待网络信号，不在GUI线程等待。QTimer到8秒调用abort，防止工作无限等。它是当前实现选择，不是所有Qt网络程序都应该这样写。
### 登录令牌与数据库密码区别
数据库密码只供服务端连接MySQL；客户端的Bearer令牌代表一个已经登录的业务用户。拿到业务令牌并不等于能直连数据库，也不应该把数据库密码当令牌发送。
ApiSession的锁保护进程内共享字符串，查询线程复制当前令牌；不是将同一个QNetworkAccessManager从GUI线程传过去。
追问：为什么断线不能返回空数组就算成功？答案：空数组表示查询成功且没有数据，断线是未知结果，必须保留错误说明。

## 面试追问与答案
1. 为什么客户端不直连数据库？中心凭据集中在服务端，可以统一权限、接口和审计。
追问 改HTTP就安全吗？不是，仍需认证、授权、TLS和输入校验。
2. 异步请求为什么还用线程？为沿用已有任务边界，局部事件循环只在工作线程运行，GUI不阻塞；并非最佳通用架构。
3. 超时会自动停止服务端SQL吗？不会，客户端abort仅停止等待，服务端仍需自己的资源限制。
4. 为什么DTO没有删除？API返回JSON后仍要转换成界面使用的数据对象，传输格式和内存对象不同。
5. 错误状态怎么处理？按HTTP状态区分未登录、无权限、依赖失败，不把所有异常当空数据。

## 练习
找到ApiClient的manager.get和EventService将JSON转换EventData的位置，说明哪一层知道HTTP、哪一层知道控件。
