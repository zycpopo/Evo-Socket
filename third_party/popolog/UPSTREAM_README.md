# popolog 日志库 — 实际开发使用指南

> 跨平台移植版（Windows / Linux / macOS）。纯头文件（header-only）C++11 日志库，无需编译成库，把 `.hpp` 拷进项目就能用。
> 已验证在 Windows 上编译运行通过（MSVC 2015+ 与 MinGW-w64 均可）。

---

## 1. 这个库能干什么

| 能力 | 说明 |
|------|------|
| 同步 / 异步 | `SyncLogger` 调用线程直接写；`AsyncLogger` 双缓冲 + 后台线程批量落地 |
| 三种落地方式 | `StdoutSink`（屏幕）、`FileSink`（文件）、`RollyBySizeSink`（按大小滚动） |
| 全局单例 | `LoggerManager` 管理所有日志器，随时按名字取回 |
| 建造者 | `LoggerBuilder` 统一配置日志器，链式装配 |
| 宏代理 | `INFO(...)` / `logger->info(...)` 自动带上文件名和行号 |

设计模式：建造者（Builder）、单例（Singleton）、代理（Proxy）、模板方法、策略（Sink 多态）、生产者-消费者（异步双缓冲）。

---

## 2. 接入（三步）

### 第 1 步：拿到头文件

`Logs-portable/` 目录下所有 `.hpp` 就是整个库（共 9 个），只需要其中一个入口：

```cpp
#include "popolog.hpp"
```

### 第 2 步：加头文件搜索路径

**方式 A — Visual Studio（图形界面）**
- 项目 → 属性 → C/C++ → 常规 → 附加包含目录：填入 `Logs-portable` 目录路径
- 项目 → 属性 → C/C++ → 语言 → C++ 语言标准：选 **C++11 或以上**
- 无需额外链接库（`std::thread` 在 MSVC 内建）

**方式 B — MinGW / g++（命令行）**

```bash
g++ main.cc -I path/to/Logs-portable -std=c++11 -pthread -o myapp.exe
```

> `-pthread` 必须有：异步日志器依赖 `std::thread`，MinGW 下需要链接 winpthreads。

**方式 C — CMake（推荐）**

```cmake
cmake_minimum_required(VERSION 3.10)
project(MyApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(myapp main.cc)
target_include_directories(myapp PRIVATE path/to/Logs-portable)

# 异步日志器需要线程库（MSVC 下自动解析为空，MinGW 下自动 -pthread）
find_package(Threads REQUIRED)
target_link_libraries(myapp PRIVATE Threads::Threads)
```

### 第 3 步：写日志

```cpp
#include "popolog.hpp"

int main() {
    INFO("hello world");              // 用默认日志器（root）打印，自动带文件:行号
    ERROR("出错了，错误码=%d", -1);
    return 0;
}
```

编译运行后，屏幕上会输出类似：

```
[22:23:01][1][root][main.cc:5][INFO]	hello world
[22:23:01][1][root][main.cc:6][ERROR]	出错了，错误码=-1
```

---

## 3. 快速上手（正式一点的例子）

```cpp
#include "popolog.hpp"

int main() {
    // 1. 用建造者装配一个"全局异步日志器"，注册进单例
    std::unique_ptr<popolog::LoggerBuilder> b(new popolog::GlobalLoggerBuilder());
    b->buildLoggerName("app");                                // 日志器名字
    b->buildLoggerLevel(popolog::LogLevel::value::DEBUG);     // 最低输出等级
    b->buildLoggerType(popolog::LoggerType::LOGGER_ASYNC);    // 同步 / 异步
    b->buildFormatter("[%d{%Y-%m-%d %H:%M:%S}][%t][%c][%f:%l][%p]%T%m%n");
    b->buildSink<popolog::FileSink>("./logs/app.log");        // 落地到文件
    b->buildSink<popolog::StdoutSink>();                      // 同时打到屏幕
    b->build();

    // 2. 通过单例按名字取回日志器
    auto lg = popolog::getLogger("app");
    lg->debug("%s", "一条 debug");
    lg->info("用户 %s 登录，ID=%d", "tom", 42);
    lg->warn("磁盘剩余不足，仅剩 %d MB", 128);

    // 3. 每次程序启动，./logs/ 目录不存在会自动创建
    return 0;
}
```

---

## 4. 核心 API 速查

### 日志等级 `LogLevel`

```cpp
popolog::LogLevel::value  // 枚举: UNKNOW, DEBUG, INFO, WARN, ERROR, FATAL, OFF
```

设置等级后，**低于该等级的日志会被丢弃**（比如设 `WARN`，`debug/info` 不打）。

### 日志器与建造者

| 类 | 作用 |
|----|------|
| `Logger` | 抽象日志器，`Logger::ptr = std::shared_ptr<Logger>` |
| `SyncLogger` | 同步日志器，调用线程直接写 |
| `AsyncLogger` | 异步日志器，写入缓冲区，后台线程批量落地 |
| `LoggerBuilder` | 建造者基类，链式配置 |
| `LocalLoggerBuilder` | 建造后**不**进全局单例，自用 |
| `GlobalLoggerBuilder` | 建造后**自动注册**进 `LoggerManager` |
| `LoggerManager` | 单例，`getLogger(name)` 取日志器 |

建造者可用方法：

```cpp
b->buildLoggerName("name");                 // 名字
b->buildLoggerLevel(LogLevel::value::WARN); // 等级
b->buildLoggerType(LoggerType::LOGGER_ASYNC | LOGGER_SYNC);
b->buildFormatter("[%d{%H:%M:%S}][%p]%m%n");// 格式模板
b->buildSink<FileSink>("./logs/a.log");     // 加落地方式，可加多个
b->buildSink<RollyBySizeSink>("./logs/roll-", 1024*1024);
b->buildSink<StdoutSink>();
b->build();                                 // 出成品
```

### 落地方式（Sink）

| Sink | 构造参数 | 行为 |
|------|----------|------|
| `StdoutSink` | 无 | 输出到标准输出 |
| `FileSink` | 文件路径 | 追加写入，自动建目录 |
| `RollyBySizeSink` | 基础名 + 单文件上限字节 | 超限自动滚动到新文件 |

### 格式化模板

`%d{子格式}` 时间、`%t` 线程ID、`%c` 日志器名、`%f` 文件、`%l` 行号、`%p` 等级、`%T` 制表符、`%m` 消息、`%n` 换行。

---

## 5. 常用场景

### 场景一：单文件小工具（不建日志器，直接用 root）

```cpp
#include "popolog.hpp"
int main() {
    INFO("开始干活");
    FATAL("致命错误，退出");
    return 0;
}
```

### 场景二：正式项目（全局异步 + 文件 + 滚动）

```cpp
std::unique_ptr<popolog::LoggerBuilder> b(new popolog::GlobalLoggerBuilder());
b->buildLoggerName("main");
b->buildLoggerLevel(popolog::LogLevel::value::INFO);
b->buildLoggerType(popolog::LoggerType::LOGGER_ASYNC);
b->buildFormatter("[%d{%Y-%m-%d %H:%M:%S}][%t][%c][%f:%l][%p]%T%m%n");
b->buildSink<popolog::RollyBySizeSink>("./logs/main-", 10 * 1024 * 1024); // 每 10MB 滚动
b->build();

auto log = popolog::getLogger("main");
log->info("服务启动");
```

### 场景三：模块内局部日志器（不进全局单例）

```cpp
std::unique_ptr<popolog::LoggerBuilder> b(new popolog::LocalLoggerBuilder());
b->buildLoggerName("net");
b->buildSink<popolog::FileSink>("./logs/net.log");
auto net = b->build();   // 自己持有，不注册到全局
net->error("连接失败: %s", "timeout");
```

---

## 6. 实际开发注意事项（重点）

1. **宏会劫持同名标识符**（最易踩的坑）
   `popolog.hpp` 无条件定义了 `#define debug/info/warn/error/fatal` 和 `DEBUG/INFO/WARN/ERROR/FATAL`。
   一旦 `#include "popolog.hpp"`，你项目里所有叫 `debug`、`info`、`error` 等**同名函数、变量、宏都会被替换**。
   - 对策 1：全局统一用大写宏 `INFO(...)` / `ERROR(...)`，不写同名的成员/函数；
   - 对策 2：不想被劫持，就**不 include `popolog.hpp`**，改为 include `logger.hpp`，直接用
     `LoggerManager` + `Logger::ptr` 的成员函数（此时 `logger->info(__FILE__, __LINE__, fmt, ...)` 要手动传文件行号）。

2. **MSVC 必须 ≥ 2015**
   用了 C99 的 `vsnprintf(nullptr, 0, ...)` 和 `va_copy`，VS2015 之前不支持。用老 VS 会编译报错。

3. **异步日志器退出时机**
   `AsyncLooper` 析构会 `stop()`（等后台线程把缓冲写完再退出），所以**程序结束前不要再写日志**，让日志器自然析构即可。
   极端场景：程序退出瞬间，另一个线程还卡在 `push()` 等待缓冲区空间，理论上有死锁风险。实际使用避开即可（main 返回前不再调用日志）。

4. **Release（NDEBUG）下文件打开失败会静默**
   `FileSink`/`RollyBySizeSink` 打开文件用 `assert` 校验。Debug 下失败会崩；Release 下 `assert` 被禁用，**文件没写成功也不报错**。
   排查"日志没落盘"时，先确认 `./logs` 路径可写、无权限问题。

5. **中文乱码问题**
   库本身写字节流，不乱码；乱码取决于源文件编码与控制台代码页。MSVC 下确保源文件保存为 UTF-8（带 BOM），或统一用 GBK。文件日志不受控制台影响。

6. **性能理解**
   无论是同步还是异步，**格式化都发生在调用线程**（`Logger::serialize` 先格式化再落地）。异步只把"写盘/输出"挪到后台线程。高频打日志时，减少格式化开销的方法是降低日志量，而不是只切异步。

7. **敏感信息不要进日志**（密码、token 等），日志文件按大小滚动后会有历史副本，删不掉就留痕。

---

## 7. FAQ

**Q: 为什么 MinGW 要加 `-pthread`，MSVC 不用？**
`std::thread` 在 MSVC 内建；MinGW 用的是 winpthreads，需要 `-pthread` 链接。CMake 的 `Threads::Threads` 会自动处理。

**Q: 同步和异步怎么选？**
- 低频、调试、要"写到哪看哪" → 同步；
- 高频、I/O 密集、不想阻塞业务线程 → 异步。
- 异步下 `buildEnableUnSafeAsync()` 可关闭缓冲满时的阻塞等待（生产者永不阻塞，但缓冲会无界增长，不推荐在持续高峰时用）。

**Q: 想把库源码放进自己的项目目录而不是引用外部路径？**
直接把 `Logs-portable` 下 9 个 `.hpp` 拷到项目 `third_party/popolog/`，include 路径指过去即可。纯头文件，没有额外构建步骤。

**Q: 没看到日志输出？**
1) 等级是否被过滤（设了 `WARN` 就不会有 `debug/info`）；2) Release 下文件是否真的打开成功（见注意点 4）；3) 是否用了 `rootLogger` 但没接文件 sink（root 默认只打屏幕）。

---

## 8. 与原版 Logs/ 的区别

| | `Logs/`（原版） | `Logs-portable/`（本版） |
|---|---|---|
| 平台 | 仅 POSIX/Linux | Windows + Linux + macOS |
| 格式化 | `vasprintf`（需 `_GNU_SOURCE`） | `util::vFormat`（跨平台） |
| 建目录 | `mkdir`（双参） | `util::makeDir`（平台分支） |
| 时间 | `localtime_r` | `util::localtimeSafe`（MSVC 分支） |
| 用途 | 设计模式学习参考 | 实际开发使用 |
