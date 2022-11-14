#ifndef _MEMRAY_COMMON_H
#define _MEMRAY_COMMON_H

#include "SPSCQueueOPT.h"
#include <atomic>
#include <chrono>
#include <ctime>
#include <sstream>
#include <string>

namespace memray {

#define __MY_DEBUG
#ifdef __MY_DEBUG
#    define MY_DEBUG(format, args...)                                                                   \
        printf("FILE: " __FILE__ ", LINE: %d: " format "\n", __LINE__, ##args)  // __VA_ARGS__ -> args
#else
// #    define MY_DEBUG(format, ...) printf("FILE: " __FILE__ ", LINE: %d: " format "\n", __LINE__)
#    define MY_DEBUG(format, ...)
#endif

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

class DebugInfo
{
public:
    int test;
    static thread_local size_t total_used_msg_node_num;

    static void printDebugCnt()
    {
        MY_DEBUG("total_used_msg_node_num: %llu", total_used_msg_node_num);
    }
};

struct Timer
{
    std::chrono::time_point<std::chrono::steady_clock> start, end;
    std::chrono::duration<float> duration;
    Timer()
    {
        start = std::chrono::steady_clock::now();
    }

    float elapsedMs()
    {
        end = std::chrono::steady_clock::now();
        duration = end - start;
        float ms = duration.count() * 1000.0f;
        return ms;
    }

    size_t elapsedS()
    {
        end = std::chrono::steady_clock::now();
        duration = end - start;
        float ms = duration.count();
        return (size_t)ms;
    }
    ~Timer()
    {
    }
};

class SpinMutex
{
  public:
    SpinMutex() = default;
    SpinMutex(const SpinMutex&) = delete;
    SpinMutex& operator=(const SpinMutex&) = delete;
    void lock()
    {
        while (flag.test_and_set(std::memory_order_acquire))
            ;
    }
    void unlock()
    {
        flag.clear(std::memory_order_release);
    }

  private:
    std::atomic_flag flag = ATOMIC_FLAG_INIT;
};

class UserThreadMutex
{
  public:
    static inline void lock()
    {
        isActive = true;
    }
    static inline void unlock()
    {
        isActive = false;
    }
    static inline bool isLocked()
    {
        return isActive;
    }
    static std::atomic<bool> isActive;
};

}  // namespace memray
#endif  //_MEMRAY_COMMON_H