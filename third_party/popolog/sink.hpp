/*日志落地模块的实现*/
#ifndef __M_SINK_H__
#define __M_SINK_H__
#include "util.hpp"

#include <memory>
#include <fstream>
#include <cassert>
#include <sstream>
#include <atomic>
#include <chrono>
#include <stdexcept>


namespace popolog
{
    class LogSink{
        public:
            using ptr = std::shared_ptr<LogSink>;
            LogSink(){}
            virtual ~LogSink() {}
            virtual void log(const char *data,size_t len) = 0;
            virtual void flush() {}
    };

    //输出到屏幕
    class StdoutSink : public LogSink{
        public:
            void log(const char *data,size_t len)
            {
                std::cout.write(data,len);
            }
            void flush() override { std::cout.flush(); }
    };

    //输出到文件
    class FileSink :public LogSink{
        public:
            //构造时传入文件名，并打开文件，将操作句柄管理起来
            FileSink(const std::string &pathname):_pathname(pathname)
            {
                //1.创建目录
                util::File::createDirectory(util::File::path(pathname));
                //2.创建并打开日志文件
                _ofs.open(_pathname,std::ios::binary | std::ios::app);
                if (!_ofs.is_open()) throw std::runtime_error("cannot open log file: " + _pathname);
            }
            void log(const char *data,size_t len)
            {
                _ofs.write(data,len);
                if (!_ofs.good()) throw std::runtime_error("cannot write log file: " + _pathname);
            }
            void flush() override {
                _ofs.flush();
                if (!_ofs.good()) throw std::runtime_error("cannot flush log file: " + _pathname);
            }
        private:
            std::string _pathname;
            std::ofstream _ofs;
    };
    
    //输出到滚动文件（以大小进行滚动）
    class RollyBySizeSink :public LogSink{
        public:
            //构造时传入文件名，并打开文件，将操作句柄管理起来
            RollyBySizeSink(const std::string &basename,size_t max_size):
                _basename(basename),max_fsize(max_size),cur_fsize(0),name_count(0)
            {
                if (max_size == 0) throw std::invalid_argument("zero log rotation size");
                std::string pathname = createNewFile();
                util::File::createDirectory(util::File::path(pathname));
                _ofs.open(pathname,std::ios::binary | std::ios::app);
                if (!_ofs.is_open()) throw std::runtime_error("cannot open rolling log: " + pathname);
            }
            //将日志信息写入到标准输出中，写之前判断一下有没有超过最大文件限制
            void log(const char *data,size_t len)
            {
                if(cur_fsize >= max_fsize)
                {
                    _ofs.close();
                    std::string pathname = createNewFile();
                    _ofs.open(pathname,std::ios::binary | std::ios::app);
                    if (!_ofs.is_open()) throw std::runtime_error("cannot open rolling log: " + pathname);
                    cur_fsize = 0;
                }
                _ofs.write(data,len);
                if (!_ofs.good()) throw std::runtime_error("cannot write rolling log: " + _basename);
                cur_fsize += len;
            }
            void flush() override {
                _ofs.flush();
                if (!_ofs.good()) throw std::runtime_error("cannot flush rolling log: " + _basename);
            }
        private:
            std::string  createNewFile()
            {
                time_t t = util::Date::getTime();
                struct tm lt;
                util::localtimeSafe(t, &lt);
                std::stringstream filename;
                filename << _basename;
                filename << lt.tm_year + 1900;
                filename << lt.tm_mon + 1;
                filename << lt.tm_mday;
                filename << lt.tm_hour;
                filename << lt.tm_min;
                filename << lt.tm_sec;
                // Separate logger instances/restarts within the same second.
                static std::atomic<size_t> sequence(0);
                filename << "-" << std::chrono::high_resolution_clock::now().time_since_epoch().count();
                filename << "-" << sequence.fetch_add(1);
                filename << "+";
                filename << name_count++;
                filename << ".log";

                return filename.str();

            }
        private:
            //通过基础文件名 + 扩展文件名（以时间生成）组成一个实际的当前输出文件名
            std::string _basename;
            std::ofstream _ofs;
            size_t max_fsize;
            size_t cur_fsize;
            size_t name_count;
    };
    
    
    class SinkFactory{
        public:
            template <typename SinkType,typename ...Args>
            static LogSink::ptr create(Args &&...args){
                return std::make_shared<SinkType>(std::forward<Args>(args)...);
            }
        
    };
}

#endif
