#ifndef _MEMRAY_LOGGING_H
#define _MEMRAY_LOGGING_H

#include <sstream>
#include <string>
#include <chrono>
#include <ctime>
#include <atomic>

namespace memray {

#define __MY_DEBUG
#ifdef __MY_DEBUG
#define MY_DEBUG(format, args...) printf("FILE: " __FILE__ ", LINE: %d: " format "\n", __LINE__, ##args)  // __VA_ARGS__ -> args
#else 
#define MY_DEBUG(format, ...) printf("FILE: " __FILE__ ", LINE: %d: " format "\n", __LINE__)
#endif

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

enum logLevel {
    NOTSET = 0,
    DEBUG = 10,
    INFO = 20,
    WARNING = 30,
    ERROR = 40,
    CRITICAL = 50,
};

void
logToStderr(const std::string& message, int level);

void
setLogThreshold(int threshold);

logLevel
getLogThreshold();

class LOG
{
  public:
    // Constructors
    LOG()
    : msgLevel(INFO){};

    explicit LOG(logLevel type)
    {
        msgLevel = type;
    };

    // Destructors
    ~LOG()
    {
        logToStderr(buffer.str(), msgLevel);
    };

    // Operators
    template<typename T>
    LOG& operator<<(const T& msg)
    {
        if (msgLevel < getLogThreshold()) {
            return *this;
        }
        buffer << msg;
        return *this;
    };

  private:
    // Data members
    std::ostringstream buffer;
    logLevel msgLevel = DEBUG;
};

struct Timer {
    std::chrono::time_point<std::chrono::steady_clock> start, end;
    std::chrono::duration<float> duration;
    Timer()
    {
        start = std::chrono::steady_clock::now();
    }

    float elapsedMs() {
        end = std::chrono::steady_clock::now();
        duration = end - start;
        float ms = duration.count() * 1000.0f;
        return ms;
    }

    size_t elapsedS() {
        end = std::chrono::steady_clock::now();
        duration = end - start;
        float ms = duration.count();
        return (size_t)ms;
    }
    ~Timer() {}
};

class SpinMutex {
public:
    SpinMutex() = default;
    SpinMutex(const SpinMutex&) = delete;
    SpinMutex& operator=(const SpinMutex&) = delete;
    void lock() {
        while(flag.test_and_set(std::memory_order_acquire));
    }
    void unlock() {
        flag.clear(std::memory_order_release);
    }

private:
    std::atomic_flag flag = ATOMIC_FLAG_INIT;
};

}  // namespace memray

#endif  //_MEMRAY_LOGGING_H
