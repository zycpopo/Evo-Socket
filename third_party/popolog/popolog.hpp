#ifndef __M_POPOLOG_H__
#define __M_POPOLOG_H__

#include "logger.hpp"

namespace popolog{
    //1.提供获取指定日志器的全局接口
    inline Logger::ptr getLogger(const std::string &name)
    {
        return popolog::LoggerManager::getInstance().getLogger(name);
    }

    inline Logger::ptr rootLogger()
    {
        return popolog::LoggerManager::getInstance().rootLogger();
    }

    //2.使用宏函数对日志器的接口进行代理（代理模式）
    #define debug(fmt,...) debug(__FILE__,__LINE__,fmt,##__VA_ARGS__)
    #define info(fmt,...) info(__FILE__,__LINE__,fmt,##__VA_ARGS__)
    #define warn(fmt,...) warn(__FILE__,__LINE__,fmt,##__VA_ARGS__)
    #define error(fmt,...) error(__FILE__,__LINE__,fmt,##__VA_ARGS__)
    #define fatal(fmt,...) fatal(__FILE__,__LINE__,fmt,##__VA_ARGS__)


    //3.提供宏函数，直接通过默认日志器进行日志的标准输出打印（没有获取日志器，直接大写打印）
    #define DEBUG(fmt,...) popolog::rootLogger()->debug(fmt,##__VA_ARGS__)
    #define INFO(fmt,...) popolog::rootLogger()->info(fmt,##__VA_ARGS__)
    #define WARN(fmt,...) popolog::rootLogger()->warn(fmt,##__VA_ARGS__)
    #define ERROR(fmt,...) popolog::rootLogger()->error(fmt,##__VA_ARGS__)
    #define FATAL(fmt,...) popolog::rootLogger()->fatal(fmt,##__VA_ARGS__)
}




#endif