/*异步日志缓冲区*/

#ifndef __M_BUFFER_H__
#define __M_BUFFER_H__

#include "util.hpp"
#include <cassert>
#include <vector>
#include <algorithm>
#include <limits>
#include <stdexcept>
namespace popolog
{
    #define BUFFER_SIZE (10*1024*1024)
    #define THRESHOLD_BUFFER_SIZE (80*1024*1024)
    #define INCREMENT_BUFFER_SIZE (10*1024*1024)
    class Buffer{
        public:
            Buffer():_buffer(BUFFER_SIZE),_reader_idx(0),_writer_idx(0){}
            //向缓冲区写入数据
            void push(const char *data,size_t len)
            {
                //空间不足就扩容
                //if(len > writeAbleSize())
                ensureEnouthSize(len);
                std::copy(data, data + len, &_buffer[_writer_idx]);
                moveWriter(len);
            }
            //返回可写数据的长度
            size_t writeAbleSize()
            {
                //这个接口仅仅针对固定大小缓冲区提供
                return (_buffer.size() - _writer_idx);
            }
            
            //返回可读数据的起始地址
            const char* begin()
            {
                return &_buffer[_reader_idx];
            }
            
            //返回可读数据的长度
            size_t readAbleSize()
            {
                return (_writer_idx - _reader_idx);
            }
            
            
            //对读写指针进行向后偏移
            void moveReader(size_t len)
            {
                assert(len <= readAbleSize());
                _reader_idx += len;
            }
            
            //重置读写位置，初始化缓冲区
            void reset()
            {
                _writer_idx = 0;//表示所有时间都是空闲的
                _reader_idx = 0;
            }
            
            //交换buffer
            void swap(Buffer &buffer)
            {
                _buffer.swap(buffer._buffer);
                std::swap(_reader_idx,buffer._reader_idx);
                std::swap(_writer_idx,buffer._writer_idx);
            }

            //判断缓冲区是否为空
            bool empty()
            {
                return (_reader_idx == _writer_idx);
            }
        private:
            //对空间进行扩容
            void ensureEnouthSize(size_t len)
            {
                if (len <= writeAbleSize()) return;
                if (len > std::numeric_limits<size_t>::max() - _writer_idx)
                    throw std::length_error("log buffer size overflow");
                const size_t required = _writer_idx + len;
                const size_t growth = _buffer.size() < THRESHOLD_BUFFER_SIZE
                    ? _buffer.size() : INCREMENT_BUFFER_SIZE;
                const size_t new_size = std::max(required, _buffer.size() + growth);
                _buffer.resize(new_size);
            }
            void moveWriter(size_t len)
            {
                assert((len+_writer_idx) <= _buffer.size());
                _writer_idx +=len;
            }
        private:
            std::vector<char> _buffer;
            size_t _reader_idx;//当前可读数据的指针--本质就是下标
            size_t _writer_idx;//当前可写数据的指针
    };
}

#endif