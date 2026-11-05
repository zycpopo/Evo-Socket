#ifndef __M_FMT_H__
#define __M_FMT_H__

#include <memory>
#include <vector>
#include <tuple>
#include <cassert>
#include <sstream>
#include <stdexcept>

#include "util.hpp"
#include "message.hpp"
#include "level.hpp"

namespace popolog{

    //抽象格式化子项基类
    class FormatItem{
        public:
            using ptr = std::shared_ptr<FormatItem>;
            virtual ~FormatItem() {} 
            virtual void format(std::ostream &out,const LogMsg &msg) = 0;
    };

    //派生格式化子项子类
    //消息
    class MsgFormatItem : public FormatItem{
        public:
            void format (std::ostream &out,const LogMsg &msg) override{
                out << msg._payload;
            }
    };
    //等级
    class LevelFormatItem : public FormatItem{
        public:
            void format (std::ostream &out,const LogMsg &msg) override{
                out << LogLevel::tostring(msg._level);
            }
    };
    //时间
    class TimeFormatItem : public FormatItem{
        public:
            TimeFormatItem(const std::string &fmt = "%H:%M:%S"): _time_fmt(fmt){}
            void format (std::ostream &out,const LogMsg &msg) override{
                struct tm t;
                util::localtimeSafe(msg._ctime,&t);
                char tmp[32] = {0};
                strftime(tmp,31,_time_fmt.c_str(),&t);
                out << tmp;
            }
        private:
            std::string _time_fmt;
    };
    //文件名
    class FileFormatItem : public FormatItem{
        public:
            void format (std::ostream &out,const LogMsg &msg) override{
                out << msg._file;
            }
    };
    //行号
    class LineFormatItem : public FormatItem{
        public:
            void format (std::ostream &out,const LogMsg &msg) override{
                out << msg._line;
            }
    };
    //线程ID
    class ThreadFormatItem : public FormatItem{
        public:
            void format (std::ostream &out,const LogMsg &msg) override{
                out << msg._tid;
            }
    };
    //日志器名称
    class LoggerFormatItem : public FormatItem{
        public:
            void format (std::ostream &out,const LogMsg &msg) override{
                out << msg._logger;
            }
    };
    //制表符
    class TabFormatItem : public FormatItem{
        public:
            void format (std::ostream &out,const LogMsg &) override{
                out << "\t";
            }
    };
    //换行
    class NLineFormatItem : public FormatItem{
        public:
            void format (std::ostream &out,const LogMsg &) override{
                out << "\n";
            }
    };
    //其它
    class OtherFormatItem : public FormatItem{
        public:
            OtherFormatItem(const std::string &str): _str(str){}
            void format (std::ostream &out,const LogMsg &) override{
                out << _str;
            }
        private:
            std::string _str;
    };

/*
    %d 表示日期，包含子格式{%H:%M:%S}
    %t 表示线程ID
    %c 表示日志器名称
    %f 表示源码文件名
    %l 表示源码行号
    %p 表示日志级别
    %T 表示制表符缩进
    %m 表示主体消息
    %n 表示换行
*/
    class Formatter{
        public:
            using ptr = std::shared_ptr<Formatter>;
            Formatter(const std::string &pattern = "[%d{%H:%M:%S}][%t][%c][%f:%l][%p]%T%m%n"):
                _pattern(pattern){
                    if (!parsePattern()) throw std::invalid_argument("invalid log format");
                }
            
            //对msg进行格式化
            void format(std::ostream &out,const LogMsg &msg){
                for (auto &item : _items)
                {
                    item->format(out,msg);
                }
            }
            std::string format(const LogMsg &msg)
            {
                std::stringstream ss;
                format(ss,msg);
                return ss.str();
            }
        private:
            //对格式化规则字符串进行解析
            bool parsePattern()
            {
                std::vector<std::pair<std::string,std::string>> fmt_order;
                size_t pos=0;
                std::string key,val;
                while(pos < _pattern.size())
                {
                    //判断是不是原始字符串
                    if(_pattern[pos]!='%'){
                        val.push_back(_pattern[pos++]);
                        continue;
                    }
                    //处理连续两个%的情况（原始字符串的情况）
                    if(pos+1 < _pattern.size() && _pattern[pos+1] == '%'){
                        val.push_back('%');
                        pos+=2;
                        continue;
                    }
                    //走到这代表原始字符串处理完毕
                    if(val.empty() == false) {
                        fmt_order.push_back(std::make_pair("",val));
                        val.clear();
                    }
                    //这时候pos位置指的是%
                    pos += 1;
                    //判断%是不是最后一个字符
                    if(pos == _pattern.size()){
                        std::cout << "%之后，没有对应的格式化字符！\n";
                        return false;
                    }
                    key = _pattern[pos];
                    pos +=1;//指向格式化字符后面的位置
                    if(pos < _pattern.size() && _pattern[pos] == '{')
                    {
                        pos+=1;
                        while(pos < _pattern.size() && _pattern[pos] != '}')
                        {
                            val.push_back(_pattern[pos++]);
                        }
                        if(pos == _pattern.size())
                        {
                            std::cout << "子规则{}匹配出错！\n";
                            return false;
                        }
                        pos+=1;//指向下次要处理新的位置
                    }
                    fmt_order.push_back(std::make_pair(key,val));
                    key.clear();
                    val.clear();
                }

                if (!val.empty()) fmt_order.push_back(std::make_pair(std::string(), val));
                for(auto &it :fmt_order)
                {
                    _items.push_back(createItem(it.first,it.second));
                }

                return true;
            }
        
            //根据不同的格式化字符创建不同的格式化子项对象
            FormatItem::ptr createItem(const std::string &key,const std::string &val){
                if(key == "d") return std::make_shared<TimeFormatItem>(val);
                if(key == "t") return std::make_shared<ThreadFormatItem>();
                if(key == "c") return std::make_shared<LoggerFormatItem>();
                if(key == "f") return std::make_shared<FileFormatItem>();
                if(key == "l") return std::make_shared<LineFormatItem>();
                if(key == "p") return std::make_shared<LevelFormatItem>();
                if(key == "T") return std::make_shared<TabFormatItem>();
                if(key == "m") return std::make_shared<MsgFormatItem>();
                if(key == "n") return std::make_shared<NLineFormatItem>();
                if(key == "") return std::make_shared<OtherFormatItem>(val);
                std::cout <<  "没有对应的格式化字符：% "<< key <<std::endl;
                abort();
                return FormatItem::ptr();
            }
        private:
            std::string _pattern;//格式化规则字符串
            std::vector<FormatItem::ptr> _items;
    };

}

#endif
