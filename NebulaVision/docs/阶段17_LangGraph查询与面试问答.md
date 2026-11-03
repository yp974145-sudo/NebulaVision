# 阶段17 LangGraph业务查询与面试问答

## 目标与实际结果
在原MainWidget顶部增加查询助手，通过当前登录会话查询设备或某天事件。报告附GET接口来源、UTC时间、事件ID与录像ID。LangGraph运行解析、查询、校验、报告四个节点，SQLite保存进度，失败后可以恢复。管理员和普通用户已分别与真实API对账通过。

本阶段采用规则解析和模板报告，不调用大语言模型，没有开放任意自然语言问答或写操作。不能把它描述成已经完成自主规划的LLM Agent。

## 文件与调用链
view/mainwidget.cpp把原来未实现的设置按钮变成查询助手，创建输入框、输出框和QProcess。
agent/client.py从标准输入读问题、任务编号和当前令牌，载入外部依赖，调用workflow.py。
agent/workflow.py定义状态和LangGraph节点；requirements.txt固定本次依赖版本。
tests/agent_workflow_check.py验证受控失败和恢复；tests/agent_live_check.py验证真实API对账。

调用顺序：点击查询 → QProcess启动Python → stdin交付请求 → GET /api/me校验身份 → 解析问题 → 白名单GET业务API → 检查证据字段 → 生成报告 → stdout交回Qt。

## 逐段讲代码
QueryState是一个字典结构的类型说明。question存问题，path存接口路径，evidence存查询结果，report存报告文字。total=False表示节点可以只返回自己负责更新的字段，不必每次复制整份状态。

BusinessTools保存当前令牌，get只允许三个明确的查询入口。问题文本不能指定新地址，不能执行SQL，也不能触发录像、删除或退出登录。真正的账号资源权限仍由原后端核验。

parse_question先识别设备列表；今日事件转换成UTC今天；其余只接受YYYY-MM-DD 事件。正则限制形式，datetime再检查日期是否真实存在，例如2月30日会被拒绝。

build_graph中的query函数调用现有API，verify检查列表和总数字段，report根据证据拼文字。graph.add_edge表示先后顺序，不是另起一个线程。compile把这些节点连接成可运行的图，并绑定SqliteSaver。

run_query先调用/api/me，再把账号ID和任务ID组合计算SHA256，作为thread_id。同名任务在不同账号下分开保存。这里SHA256用于稳定命名，不用于登录密码加密。

SqliteSaver把节点执行后的状态保存在磁盘。graph.get_state读取上一次进度；previous.next非空说明仍有未执行节点，invoke(None)继续运行。失败前完成的解析可以保留，失败的查询会重试。工具只有只读查询，所以重复执行不会重复写入业务事件。

已完成任务再次点击会重新查询，不把历史报告冒充最新结果。同一任务换问题被拒绝；Qt里问题发生变化会生成新UUID。

Qt中的QProcess独立运行Python，GUI不用同步等待。started信号触发时把JSON写入stdin并关闭写入端；finished信号触发时解析stdout。令牌没有放进命令行或checkpoint。finished的Lambda捕获dialog内控件，连接的context是dialog，窗口销毁后Qt会断开这些连接。

20秒QTimer限制整个助手进程时间；单独网络查询超时为5秒。关闭窗口时结束助手进程。会话改变后，Qt不会展示旧会话的报告。Python异常只返回类别与通用说明，不把可能含敏感信息的异常原文直接显示。

## 测试与边界
受控测试通过：查询超时后恢复、账号checkpoint隔离、过期会话拒绝、非法日期、写操作拒绝、问题变更拒绝、事件总数与展示条数区分。真实管理员和普通用户的设备数、事件总数分别与直接API查询一致。Qt6 Release重新编译通过。

原Qt窗口到Python、真实API报告及文档检索的offscreen集成验证已通过；尚未完成真实屏幕鼠标交互、长时间并发压测、其他机器Python路径适配和大语言模型接入。checkpoint包含业务查询结果，属于本机运行数据，应限制本机文件权限并在交付中排除；本阶段尚无自动保留期清理。恢复会重新核验登录，缓存文件本身不作为独立授权入口。当前桌面进程启动方式需要本机agent-python依赖目录。

## 面试追问与答案
问题1：LangGraph是什么？答：用节点和边表达任务流程的状态图框架。这里四个节点把解析、查询、证据检查和报告分开。追问：有图就等于LLM Agent吗？答：不等于，本阶段没有LLM，只有规则流程。

问题2：checkpoint有什么用？答：保存已执行节点的状态，程序失败后能从未完成部分继续。追问：等于数据库事务吗？答：不是，外部业务工具仍需自己的事务或幂等设计。

问题3：为什么先调用/api/me？答：恢复任务也必须确认当前登录有效，并取得账号ID隔离任务。追问：能靠task_id授权吗？答：不能，真正权限由会话及后端资源权限检查决定。

问题4：为什么事件总数不用列表长度？答：接口分页只返回最多10条，total是后端条件计数。追问：API两次查询期间数据变化怎么办？答：本阶段不是一致性快照，报告只说明查询时证据，严格审计需后端提供快照或事务读策略。

问题5：为何令牌放stdin？答：避免出现在进程命令行中，也不进入状态图持久化字段。追问：这样就是加密了吗？答：不是，是本机进程间传递，远程传输仍需TLS，文件和进程权限仍要保护。

问题6：退出窗口是否等于取消服务端查询？答：不是，终止客户端进程只能停止等待，服务器可能已经执行只读请求。追问：写工具怎么办？答：必须另做请求幂等键、服务端任务状态及取消协议，本阶段不开放写工具。

## 练习
在workflow.py中找到parse_question、query和report。输入设备列表，再输入某天事件，观察path如何决定白名单工具。输入删除所有事件，确认它被拒绝，而不是被当成SQL执行。

## 官方资料
LangGraph SqliteSaver：https://reference.langchain.com/python/langgraph.checkpoint.sqlite/SqliteSaver 。这是流程持久化的官方API说明，具体业务权限与Qt集成由本项目实现。


补充集成修复：Qt通过stdin发送UTF-8 JSON；Windows的Python默认输入编码可能不同。client.py使用sys.stdin.buffer.read后显式decode UTF-8，避免中文问题被错误解码。tests/agent_ui_check.cpp已从原主窗口完成业务查询与真实向量检索，两条链路退出码0。
