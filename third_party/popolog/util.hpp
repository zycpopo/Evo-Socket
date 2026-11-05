//实用工具类的实现
//1.获取系统时间
//2.判断文件是否存在
//3.获取文件所在路径
//4.创建目录

#ifndef __MY_UTIL_H__
#define __MY_UTIL_H__

//确保在严格模式(-std=c++11)下 glibc 仍暴露 POSIX 接口(localtime_r / mkdir)
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <iostream>
#include <ctime>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <fstream>
#include <sstream>
#include <cassert>

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif
#include <sys/stat.h>

namespace popolog
{
    namespace util
    {
        //可移植格式化:替代 vasprintf(先计算长度再写入,三平台统一)
        inline std::string vFormat(const std::string &fmt, va_list ap)
        {
            va_list ap2;
            va_copy(ap2, ap);
            int len = vsnprintf(nullptr, 0, fmt.c_str(), ap2);
            va_end(ap2);
            //vsnprintf 失败(如编码错误)时返回空串,调用方会落一条空消息
            if(len < 0) return std::string();
            std::string str(static_cast<size_t>(len) + 1, '\0');
            vsnprintf(&str[0], str.size(), fmt.c_str(), ap);
            str.resize(static_cast<size_t>(len));
            return str;
        }
        //可移植建目录
        inline int makeDir(const std::string &path)
        {
#ifdef _WIN32
            return _mkdir(path.c_str());
#else
            return mkdir(path.c_str(), 0755);
#endif
        }
        //可移植时间转换(MSVC 的 localtime_s 参数顺序相反)
        inline void localtimeSafe(time_t t, struct tm *out)
        {
#ifdef _MSC_VER
            localtime_s(out, &t);
#else
            localtime_r(&t, out);
#endif
        }
        class Date{
            public:
                static size_t getTime(){
                    return (size_t)time(nullptr);
                }
        };
        class File{
            public:
                static bool exists(const std:: string &pathname)//2
                {
                    struct stat st;
                    if(stat(pathname.c_str(),&st) < 0)
                    {
                        return false;
                    }
                    return true;
                }
                static std::string path(const std::string &name)//3
                {
                    if (name.empty()) return ".";
                    size_t pos = name.find_last_of("/\\");
                    if (pos == std::string::npos) return ".";
                    return name.substr(0, pos + 1);
                }
                static void createDirectory(const std::string &path)//4
                {
                    if (path.empty()) return ;
                    if (exists(path)) return ;
                    size_t pos, idx = 0;
                    while(idx < path.size()) {
                        pos = path.find_first_of("/\\", idx);
                        if (pos == std::string::npos) {
                            makeDir(path);
                            return;
                        }
                        if (pos == idx) {idx = pos + 1; continue;}
                        std::string subdir = path.substr(0, pos);
                        if (subdir == "." || subdir == "..") {idx = pos + 1; continue;}
                        if (exists(subdir)) {idx = pos + 1; continue;}
                        makeDir(subdir);
                        idx = pos + 1;
                    }
                }
        };
    }
}

#endif