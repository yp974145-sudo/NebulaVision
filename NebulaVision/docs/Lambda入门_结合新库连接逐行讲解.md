# Lambda入门：结合本项目逐行理解

## 1. 它是什么，为什么学
Lambda是C++11开始支持的一种写法：在需要的位置定义一个小函数。它属于语言基础，不是需要安装的库。项目的class15.pro已经启用c++11。
普通函数一般在类里声明、在外部实现；Lambda可以直接写在另一个函数内部。适合只在当前位置使用的一小段操作。
本项目在DevDbConnection::open()内部用它统一处理失败。后续Qt按钮回调、网络响应和异步任务也可能用到，但不要求所有回调都写Lambda。

## 2. 从普通函数理解
例如普通函数：
```cpp
bool returnFalse()
{
    return false;
}
```
Lambda的对应写法：
```cpp
auto returnFalse = []() -> bool {
    return false;
};
bool result = returnFalse();
```
定义时没有执行函数体；调用returnFalse()时才执行。auto让编译器推导变量类型，因为每个Lambda都有自己的类型。它不代表多线程或自动执行。

## 3. 本项目实际代码
```cpp
auto fail = [this, error](const QString &message) -> bool {
    close();
    if (error != nullptr) {
        *error = message;
    }
    return false;
};
```
逐个解释：
- auto fail：用变量fail保存这个小函数，之后通过fail(...)调用。
- [this, error]：捕获列表。让小函数能使用当前对象和外面的error指针。
- this：当前DevDbConnection对象的指针，因此里面能调用该对象的close()。
- error：复制指针值，使它仍指向调用者提供的QString；没有复制那个QString。
- (const QString &message)：本次调用传入的错误说明；只读引用，不修改传入文字。
- -> bool：明确说明返回值是布尔类型。
- { ... }：实际执行的函数体。
- 最后的分号：结束变量定义。

## 4. error和*error到底是什么
调用者这样写：
```cpp
QString errorText;
bool success = connection.open(path, &errorText);
```
&errorText取出字符串的地址，open()里的error指向它。
```cpp
*error = message;
```
表示修改这个地址处的字符串，即给调用者的errorText赋值。它不是修改密码。
如果调用者传nullptr，表示不需要错误文字；因此先检查error != nullptr，避免访问空指针。

## 5. 执行过程
例如配置文件无法打开：
```cpp
return fail("Cannot read local database config");
```
顺序是：调用fail -> close清理连接 -> 将错误说明写进调用者字符串 -> fail返回false -> 外层return把false返回给open()调用者。
定义fail时并不连接或关闭数据库。只有走到调用处才执行。
相当于每次失败都展开写：
```cpp
close();
if (error != nullptr) {
    *error = "Cannot read local database config";
}
return false;
```
多个失败分支共用它，避免遗漏清理和错误赋值。

## 6. 捕获与参数有什么区别
```cpp
[this, error](const QString &message)
```
this和error来自定义Lambda的位置，在定义时捕获；message由每次调用fail("...")时传入。不同调用可以传入不同错误原因。
原先的[&]是按引用捕获用到的外部局部变量；现在显式列出this和error，更容易看出它依赖什么，也不依赖error这个局部指针变量本身的存活。
但复制指针不延长所指对象的寿命！本项目只在open()内部立即调用，不存起来，也不异步执行，因此对象和调用者字符串此时仍有效。

## 7. 和Qt信号槽的关系
以下只是后续教学例子，不是本阶段新增的业务代码：
```cpp
connect(button, &QPushButton::clicked, this, [this]() {
    loadEvents();
});
```
按钮点击时执行Lambda，Lambda调用当前窗口的loadEvents()。
connect负责建立联系，Lambda负责描述收到信号后做什么。Lambda本身不会创建线程。异步场景必须另外考虑对象是否仍存在；不能把本项目的fail直接拿去长期保存。

## 8. 哪些不需要现在背
不用先背底层闭包实现、模板和所有捕获语法。先能指出捕获列表、参数、函数体、调用位置就够了。

## 9. 面试问答（答案对应本次代码）
问：为什么这里使用Lambda？
答：失败处理只在open内部使用，用一个局部小函数统一关闭连接、填写错误和返回false，减少重复。
问：定义后立即执行吗？
答：不会，fail(...)时才执行。
问：[this, error]是什么？
答：显式捕获当前对象指针与错误字符串指针，供函数体使用。
问：按值捕获error后，为何还能改外面的字符串？
答：复制的是地址，两个指针仍指向同一个QString，*error修改的是目标字符串。
问：Lambda一定异步吗？
答：不是。本例同步执行，在哪个线程调用就在哪个线程运行。

## 10. 小练习
在代码中找到三个fail调用。分别说出：什么条件失败、传了哪段错误文字、谁接收到false。不需要修改真实密码或本地配置。
