#ifndef __MY_MSG_H__
#define __MY_MSG_H__

#include <iostream>
#include <string>
#include <thread>

#include "level.hpp"
#include "util.hpp"

namespace popolog{
    struct LogMsg{
        time_t _ctime;
        LogLevel::value _level; //日志等级
        size_t _line;           //行号
        std::thread::id _tid;   //线程ID
        std::string _file;      //源码文件名
        std::string _logger;    //日志器名称
        std::string _payload;   //有效消息数据

        LogMsg(LogLevel::value level,
            size_t line,
            const std::string file,
            const std::string logger,
            const std::string msg):
                _ctime(util::Date::getTime()),
                _level(level),
                _line(line),
                _tid(std::this_thread::get_id()),
                _file(file),
                _logger(logger),
                _payload(msg){}
    };
}
#endif