# popoLog 接入说明

本目录是 TcpServer 的独立改造副本。原始 `D:\Technology\Project\TcpServer` 和
`D:\Technology\Project\popoLog` 未修改。副本不复制原仓库的 `.git`、本地助手配置、
旧可执行文件和运行日志。

## 接入结构

```text
业务 / Reactor 内部的 DBG_LOG、INF_LOG、ERR_LOG
                     ↓
             include/logging.hpp
                     ↓
         third_party/popolog/logger.hpp
                     ↓
        AsyncLogger → 双缓冲 → 后台工作线程
                     ↓
           控制台 + 按大小滚动的文件
```

- `third_party/popolog/`：从本机 popoLog 复制的头文件，可独立构建，无需原路径存在。
- `include/logging.hpp`：配置和兼容适配，直接使用 popoLog 类接口，避免通用宏污染。
- `include/server_common.hpp`：移除原来的 `fprintf/localtime` 日志宏，统一包含适配层。
- `source/server.hpp`：兼容入口，转发到拆分后的 `include/server.hpp`，避免维护两套实现。
- Echo / HTTP 的 `main.cc`：启动网络服务前配置异步日志、控制台和滚动文件。
- HTTP 文件访问失败的 `printf`：改为 `ERR_LOG`，错误也会进入文件。

## 在服务中配置

```cpp
#include "server.hpp"

int main() {
    loopforge::LogConfig config;
    config.name = "my-server";
    config.level = loopforge::LogLevel::INFO;
    config.asynchronous = true;
    config.console = true;
    config.file_prefix = "./logs/my-server-";
    config.max_file_size = 10 * 1024 * 1024;
    loopforge::InitLogging(config);

    INF_LOG("server starting, port=%d", 9000);
    TcpServer server(9000);
    server.SetThreadCount(2);
    server.Start();
}
```

配置应在创建网络对象、启动业务线程之前完成。默认级别为 DEBUG，默认异步，
默认同时写控制台和 `./logs/loopforge-*.log`，目录自动创建。
没有显式初始化时，兼容宏使用同步控制台日志器，不自动创建文件。

`file_prefix` 是文件名前缀，清空即可关闭文件输出；`console=false` 关闭控制台。
必须至少启用一种输出。文件名前缀可以带绝对路径，否则相对于进程工作目录。
初始化失败会抛出异常并保留原日志器。

现有调用无需修改：

```cpp
DBG_LOG("connection=%p", static_cast<void *>(connection));
INF_LOG("accepted fd=%d", fd);
ERR_LOG("recv failed: %s", strerror(errno));
WRN_LOG("send buffer is large: %zu bytes", size);
FTL_LOG("fatal diagnostic");
```

上面是接口示例，`connection`、`fd`、`size` 由调用方提供。
`FTL_LOG` 会等待输出完成，本身不会结束进程。格式字符串使用 printf 规则。
适配层保留调用处文件和行号，并恢复调用前的 errno；被过滤的消息不求值参数。
原来的 `LOG_LEVEL` 编译期过滤改为 `LogConfig::level` 运行时配置。
标准等级顺序为 DEBUG < INFO < WARN < ERROR < FATAL < OFF。
旧的 `LOG(INF/DBG/ERR, ...)` 参数仍被正确映射。

## 输出位置

从示例目录启动时：

- Echo：`source/echo/logs/echo-*.log`。
- HTTP：`source/http/logs/http-*.log`。

每条消息包含日期时间、线程 ID、日志器名称、源码位置、级别和正文。
异步后台线程每批输出后刷新流，便于运行时查看。
滚动阈值在批次写入前检查；单次异步批次或一条大消息可能超过阈值。
当前不自动删除历史文件，不限制总磁盘占用。

## flush 和生命周期

- `loopforge::FlushLogging()`：等待队列写出并刷新流，适合检查日志或关键阶段。
- `loopforge::ShutdownLogging()`：业务写日志线程停止后调用，排空旧日志器并恢复同步控制台。
- 正常析构同样会排空队列。日志流刷新不等同于磁盘 `fsync`。
- 示例原有事件循环没有优雅停止接口；Ctrl+C、SIGTERM 默认终止、abort、崩溃和断电
  不能保证异步队列完整落盘。本次未增加网络服务退出机制。
- 安全异步模式会在缓冲区满时等待；高峰下仍可能让 Reactor 线程阻塞。
  格式化也仍在调用线程完成，本次未做吞吐量提升声明。
- 后台输出错误保存为异常，可由后续写入或 `FlushLogging()` 获取；析构不会抛出。

## 副本中对 popoLog 的修复

只修复 vendored 副本，原 popoLog 不变。细节见 `third_party/popolog/PATCHES.md`。

1. Release 构建也执行格式模板解析，避免 `assert(parsePattern())` 被移除后日志变空。
2. 异步回调先于工作线程初始化；停止时唤醒生产者和消费者，支持重复 stop。
3. 超出默认 10 MiB 缓冲区的单条消息可扩容，避免无限等待；一次扩容覆盖实际所需大小。
4. 增加 flush 和排空等待，保存并传播后台输出异常。
5. 文件打开、写入、刷新失败不再仅靠 assert；Release 同样报错。
6. 格式化使用有空间容纳结束符的缓冲，支持空消息；可变参数接口使用 `const char *`。
7. 格式模板保留末尾普通字符；补充基类虚析构和 header-only 入口 inline。
8. 滚动文件名加入高精度时间和序号，减少同一秒内重建日志器的文件重用。

## 构建和测试

网络示例仍依赖 Linux 的 epoll、eventfd、timerfd。没有变为原生 Windows 网络库。

在 Linux 项目根目录：

```sh
make
make check
bash test/run_checks.sh
```

或者使用 CMake：

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
cd build
./http_server
```

`make check` 含原 Socket 接收语义测试和新增日志测试。`run_checks.sh` 含原有 TCP/HTTP
回归检查，其连接回收断言已改为匹配实际存在的 `RELEASE CONNECTION` 日志。
日志测试可单独在 Linux 或 Windows MinGW 下运行：

```sh
g++ -std=c++11 -O2 -DNDEBUG -Wall -Wextra -pedantic -Iinclude test/logging_test.cc test/logging_peer.cc -pthread -o logging_test
./logging_test build/logging-test-output
```

在 Windows PowerShell 下将最后一行改为 `./logging_test.exe build/logging-test-output`。
日志测试每次创建独立输出子目录，包含一个约 25 MiB 的测试文件。
本机实际验证范围见 [验证记录](验证记录.md)。
