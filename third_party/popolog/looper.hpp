#ifndef __M_LOOPER_H__
#define __M_LOOPER_H__

#include "buffer.hpp"
#include <condition_variable>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

namespace popolog {
using Functor = std::function<void(Buffer &)>;
enum class AsyncType { ASYNC_SAFE, ASYNC_UNSAFE };

class AsyncLooper {
public:
    using ptr = std::shared_ptr<AsyncLooper>;
    AsyncLooper(const Functor &cb, AsyncType type = AsyncType::ASYNC_SAFE)
        : _stop(false), _writing(false), _looper_type(type), _callback(cb),
          _thread(&AsyncLooper::threadEntry, this) {}
    ~AsyncLooper() { stop(); }

    void stop() {
        std::lock_guard<std::mutex> stop_lock(_stop_mutex);
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _stop = true;
        }
        _cond_con.notify_all();
        _cond_pro.notify_all();
        if (_thread.joinable()) _thread.join();
    }

    void push(const char *data, size_t len) {
        std::unique_lock<std::mutex> lock(_mutex);
        if (_looper_type == AsyncType::ASYNC_SAFE) {
            // An oversized record may grow an empty buffer instead of waiting forever.
            _cond_pro.wait(lock, [&] {
                return _stop || _pro_buf.empty() || _pro_buf.writeAbleSize() >= len;
            });
        }
        if (_failure) std::rethrow_exception(_failure);
        if (_stop) return;
        _pro_buf.push(data, len);
        _cond_con.notify_one();
    }

    void flush() {
        std::unique_lock<std::mutex> lock(_mutex);
        _cond_drained.wait(lock, [&] {
            return _failure || (_pro_buf.empty() && !_writing);
        });
        if (_failure) std::rethrow_exception(_failure);
    }

private:
    void threadEntry() {
        for (;;) {
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _cond_con.wait(lock, [&] { return _stop || !_pro_buf.empty(); });
                if (_pro_buf.empty() && _stop) break;
                _pro_buf.swap(_con_buf);
                _writing = true;
                _cond_pro.notify_all();
            }
            try {
                _callback(_con_buf);
            } catch (...) {
                std::lock_guard<std::mutex> lock(_mutex);
                _failure = std::current_exception();
                _stop = true;
                _writing = false;
                _cond_pro.notify_all();
                _cond_drained.notify_all();
                return;
            }
            _con_buf.reset();
            {
                std::lock_guard<std::mutex> lock(_mutex);
                _writing = false;
                _cond_drained.notify_all();
            }
        }
    }

    bool _stop;
    bool _writing;
    AsyncType _looper_type;
    Buffer _pro_buf;
    Buffer _con_buf;
    std::mutex _mutex;
    std::mutex _stop_mutex;
    std::condition_variable _cond_pro;
    std::condition_variable _cond_con;
    std::condition_variable _cond_drained;
    std::exception_ptr _failure;
    Functor _callback; // Initialize the callback before starting its worker.
    std::thread _thread;
};
} // namespace popolog
#endif
