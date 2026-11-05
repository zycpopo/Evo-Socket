# Vendored popoLog

来源：本机 `D:\Technology\Project\popoLog` 的全部 9 个 `.hpp`。
原使用说明保存在 `UPSTREAM_README.md`，部分原注意事项已由本副本修复。
本副本无需引用该外部路径，原库未修改。

本地补丁：

| 文件 | 变更 |
| --- | --- |
| `logger.hpp` | printf 可变参数使用指针格式串；虚析构；级别查询；flush；异步析构先停止线程；每批刷新；避免重复构造格式结果 |
| `looper.hpp` | 回调先初始化；有锁的停止状态；唤醒阻塞生产者；排空与 flush；允许单条超大消息；后台异常传播；重复 stop 安全 |
| `buffer.hpp` | 根据消息实际长度扩容并检查长度溢出 |
| `format.hpp` | Release 仍解析模板；保留尾部普通字符；格式子项虚析构；清理未使用参数警告 |
| `util.hpp` | 为 vsnprintf 的结尾字符分配空间，正确处理空消息 |
| `sink.hpp` | 提供 flush；打开/写入/刷新失败抛出异常；滚动阈值校验；文件名加高精度时间与序号 |
| `popolog.hpp` | 自由函数改为 inline，避免多翻译单元重复定义 |

适配层仅包含 `logger.hpp`，不引入 `popolog.hpp` 中的通用宏。
没有改用其他日志库，也没有启用不受限的异步缓冲模式。
