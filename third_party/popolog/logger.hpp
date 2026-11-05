/*日志器模块*/
#ifndef __M_LOGGER_H__
#define __M_LOGGER_H__

#include "util.hpp"
#include "level.hpp"
#include "format.hpp"
#include "sink.hpp"
#include "looper.hpp"

#include <atomic>
#include <mutex>
#include <cstdarg>
#include <cassert>
#include <unordered_map>

namespace popolog{
    class Logger{
        public:
            using ptr = std::shared_ptr<Logger>;
            Logger(const std::string &logger_name,
                LogLevel::value level,
                Formatter::ptr &formatter,
                std::vector<LogSink::ptr> &sinks):
                _logger_name(logger_name),
                _limit_level(level),
                _formatter(formatter),
                _sinks(sinks.begin(),sinks.end()){}

            virtual ~Logger() {}
            bool shouldLog(LogLevel::value level) const { return level >= _limit_level.load(); }
            virtual void flush() = 0;

            const std::string & name(){
                return _logger_name;
            }
            //完成构造日志消息对象并进行格式化，得到格式化后的日志消息字符串，然后进行落地输出
            void debug(const std::string &file,size_t line,const char *fmt,...)
            {
                //1.判断当前的日志是否达到了输出等级
                if(LogLevel::value::DEBUG < _limit_level){return ;}
                va_list ap;
                va_start(ap,fmt);
                std::string msg = util::vFormat(fmt,ap);
                va_end(ap);
                serialize(LogLevel::value::DEBUG,file,line,msg);
            }
            void info(const std::string &file,size_t line,const char *fmt,...)
            {
                //1.判断当前的日志是否达到了输出等级
                if(LogLevel::value::INFO < _limit_level){return ;}
                va_list ap;
                va_start(ap,fmt);
                std::string msg = util::vFormat(fmt,ap);
                va_end(ap);
                serialize(LogLevel::value::INFO,file,line,msg);
            }
            void warn(const std::string &file,size_t line,const char *fmt,...)
            {
                //1.判断当前的日志是否达到了输出等级
                if(LogLevel::value::WARN< _limit_level){return ;}
                va_list ap;
                va_start(ap,fmt);
                std::string msg = util::vFormat(fmt,ap);
                va_end(ap);
                serialize(LogLevel::value::WARN,file,line,msg);
            }
            void error(const std::string &file,size_t line,const char *fmt,...)
            {
                //1.判断当前的日志是否达到了输出等级
                if(LogLevel::value::ERROR < _limit_level){return ;}
                va_list ap;
                va_start(ap,fmt);
                std::string msg = util::vFormat(fmt,ap);
                va_end(ap);
                serialize(LogLevel::value::ERROR,file,line,msg);
            }
            void fatal(const std::string &file,size_t line,const char *fmt,...)
            {
                //1.判断当前的日志是否达到了输出等级
                if(LogLevel::value::FATAL < _limit_level){return ;}
                va_list ap;
                va_start(ap,fmt);
                std::string msg = util::vFormat(fmt,ap);
                va_end(ap);
                serialize(LogLevel::value::FATAL,file,line,msg);
            }
        private:
            void serialize(LogLevel::value level,const std::string &file,size_t line,const std::string &str)
            {
                LogMsg msg(level,line,file,_logger_name,str);
                //4.通过格式化工具对msg进行格式化，得到格式化后的日志字符串
                std::stringstream ss;
                _formatter->format(ss,msg);
                //5.进行日志落地
                const std::string output = ss.str();
                log(output.data(), output.size());
            }    

            //抽象接口完成落地输出，根据不同的日志器会有不同的实际落地方式
            virtual void log(const char *data,size_t len) = 0;
        protected:
            std::mutex _mutex;
            std::string _logger_name;
            std::atomic<LogLevel::value> _limit_level;
            Formatter::ptr _formatter;
            std::vector<LogSink::ptr> _sinks;
    };

    class SyncLogger :public Logger
    {
        public:
            SyncLogger(const std::string &logger_name,
                LogLevel::value level,
                Formatter::ptr &formatter,
                std::vector<LogSink::ptr> &sinks):
                Logger(logger_name,level,formatter,sinks){}
            void flush() override {
                std::lock_guard<std::mutex> lock(_mutex);
                for (auto &sink : _sinks) sink->flush();
            }
        protected:
            void log(const char *data,size_t len)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                if(_sinks.empty()) return;
                for(auto &sink : _sinks)
                {
                    sink->log(data,len);
                }
            }
    };  

    class AsyncLogger : public  Logger{
        public: 
            AsyncLogger(const std::string &logger_name,
                LogLevel::value level,
                Formatter::ptr &formatter,
                std::vector<LogSink::ptr> &sinks,
                AsyncType looper_type):
                Logger(logger_name,level,formatter,sinks),
                _looper(std::make_shared<AsyncLooper>(std::bind(&AsyncLogger::realLog,this,std::placeholders::_1),looper_type)){}

            ~AsyncLogger() override { _looper->stop(); }
            void flush() override {
                _looper->flush();
                std::lock_guard<std::mutex> lock(_mutex);
                for (auto &sink : _sinks) sink->flush();
            }

            void log(const char * data,size_t len)//将数据写入缓冲区
            {
                _looper->push(data,len);
            }
            void realLog(Buffer &buf)//将缓冲区中的数据落地
            {
                std::lock_guard<std::mutex> lock(_mutex);
                if(_sinks.empty()) return;
                for(auto &sink : _sinks)
                {
                    sink->log(buf.begin(),buf.readAbleSize());
                    sink->flush();
                }
            }
        private:
            AsyncLooper::ptr  _looper;
    };

    enum class LoggerType
    {
        LOGGER_SYNC,
        LOGGER_ASYNC
    };
    class LoggerBuilder{
        public:
            virtual ~LoggerBuilder() {}
            LoggerBuilder():
                _logger_type(LoggerType::LOGGER_SYNC),
                _limit_level(LogLevel::value::DEBUG),
                _looper_type(AsyncType::ASYNC_SAFE){}
            void buildEnableUnSafeAsync()
            {
                _looper_type = AsyncType::ASYNC_UNSAFE;
            }
            void buildLoggerType(LoggerType type)
            {
                _logger_type = type;
            }
            void buildLoggerName(const std::string &name)
            {
                _logger_name = name;
            }
            void buildLoggerLevel(LogLevel::value level)
            {
                _limit_level = level;
            }
            void buildFormatter(const std::string &pattern)
            {
                _formatter = std::make_shared<Formatter>(pattern);
            }
            template<typename SinkType,typename ...Args>
            void buildSink(Args && ...args)
            {
                LogSink::ptr psink = SinkFactory::create<SinkType>(std::forward<Args>(args)...);
                _sinks.push_back(psink);
            }
            virtual Logger::ptr build() = 0;

        protected:
            LoggerType _logger_type;
            std::string _logger_name;
            std::atomic<LogLevel::value> _limit_level;
            Formatter::ptr _formatter;
            std::vector<LogSink::ptr> _sinks;
            AsyncType _looper_type;
    };

    //具体的建造者类--局部日志器的建造者
    class LocalLoggerBuilder : public LoggerBuilder{
        public:
        Logger::ptr build() override{
            assert(_logger_name.empty() == false);
            if(_formatter.get() == nullptr)
            {
                _formatter = std::make_shared<Formatter>();
            }
            if(_sinks.empty())
            {
                buildSink<StdoutSink>();
            }
            if(_logger_type == LoggerType::LOGGER_ASYNC)
            {
                return std::make_shared<AsyncLogger>(_logger_name,_limit_level,_formatter,_sinks,_looper_type);
            }
            return std::make_shared<SyncLogger>(_logger_name,_limit_level,_formatter,_sinks);
        }
    };

    class LoggerManager{
        public:
            static LoggerManager& getInstance()
            {
                //C++11后，静态局部变量，在编译层面实现了线程安全
                static LoggerManager eton;
                return eton;
            }
            void addLogger(Logger::ptr &logger)
            {
                if(hasLogger(logger->name())) return ; 
                std::unique_lock<std::mutex> lock(_mutex);
                _loggers.insert(std::make_pair(logger->name(),logger));
            }
            bool hasLogger(const std::string &name)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _loggers.find(name);
                if(it == _loggers.end()){
                    return false;
                }
                return true;
            }
            Logger::ptr getLogger(const std::string &name)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _loggers.find(name);
                if(it == _loggers.end()){
                    return Logger::ptr();
                }
                return it->second;
            }
            Logger::ptr rootLogger()
            {
                return _root_logger;
            }
        private:
            LoggerManager() {
                std::unique_ptr<popolog::LoggerBuilder> builder(new popolog::LocalLoggerBuilder());
                builder->buildLoggerName("root");
                _root_logger = builder->build();
                _loggers.insert(std::make_pair("root",_root_logger));
            }
        private:
            std::mutex _mutex;
            Logger::ptr _root_logger;//默认日志器
            std::unordered_map<std::string,Logger::ptr> _loggers;//管理所有日志器的数组
    };

    //全局日志器的建造者---在局部的基础上增加一个功能，将日志器添加到单例对象中
    class GlobalLoggerBuilder : public LoggerBuilder{
        public:
            Logger::ptr build() override{
                assert(_logger_name.empty() == false);
                if(_formatter.get() == nullptr)
                {
                    _formatter = std::make_shared<Formatter>();
                }
                if(_sinks.empty())
                {
                    buildSink<StdoutSink>();
                }
                Logger::ptr logger;
                if(_logger_type == LoggerType::LOGGER_ASYNC)
                {
                    logger = std::make_shared<AsyncLogger>(_logger_name,_limit_level,_formatter,_sinks,_looper_type);
                }
                else{
                    logger = std::make_shared<SyncLogger>(_logger_name,_limit_level,_formatter,_sinks);
                }
                LoggerManager::getInstance().addLogger(logger);
                return logger;
        }
    };
}

#endif
