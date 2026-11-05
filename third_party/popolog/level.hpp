//1.定义枚举日志等级类
//2.提供转换接口(将日志等级转换成对应字符串)

#ifndef __M_LEVEL_H__
#define __M_LEVEL_H__

namespace popolog{
    class LogLevel{
        public:
            enum class value{
                UNKNOW =0,
                DEBUG,
                INFO,
                WARN,
                ERROR,
                FATAL,
                OFF
            };//1

            static const char *tostring(LogLevel::value level)//2
            {
                switch(level)
                {
                    case LogLevel::value::DEBUG: return "DEBUG";
                    case LogLevel::value::INFO:  return "INFO";
                    case LogLevel::value::WARN:  return "WARN";
                    case LogLevel::value::ERROR: return "ERROR";
                    case LogLevel::value::FATAL: return "FATAL";
                    case LogLevel::value::OFF:   return "OFF";
                    default:
                        return "UNKNOW";
                }
            }
    };
}

#endif