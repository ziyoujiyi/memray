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
    static thread_local size_t total_used_msg_node;
    static thread_local size_t total_processed_msg;

    static thread_local size_t blocked_cpu_sample;
    static thread_local size_t processed_cpu_sample;
    static thread_local size_t blocked_allocation;
    static thread_local size_t processed_allocation;

    static thread_local size_t write_unresolvednativeframe_msg;
    static thread_local size_t write_frame_push_msg;
    static thread_local size_t write_frame_pop_msg;
    static thread_local size_t write_allocation_msg;
    static thread_local size_t write_native_allocation_msg;
    static thread_local size_t write_pyrawframe_msg;
    static thread_local size_t write_memory_record_msg;
    static thread_local size_t write_segment_header_msg;
    static thread_local size_t write_segment_msg;

    static thread_local size_t read_unresolvednativeframe_msg;

    static thread_local size_t add_cpu_sample;
    static thread_local size_t add_allocation;

    static void printWriteDebugCnt()
    {
        MY_DEBUG("write_segment_header_msg: %llu", write_segment_msg);
        MY_DEBUG("write_segment_msg: %llu", write_segment_msg);
        MY_DEBUG("total_used_msg_node: %llu", total_used_msg_node);
        MY_DEBUG("blocked_cpu_sample: %llu", blocked_cpu_sample);
        MY_DEBUG("processed_cpu_sample: %llu", processed_cpu_sample);
        MY_DEBUG("blocked_allocation: %llu", blocked_allocation);
        MY_DEBUG("processed_allocation: %llu", processed_allocation);
        MY_DEBUG("write_unresolvednativeframe_msg: %llu", write_unresolvednativeframe_msg);
        MY_DEBUG("write_frame_push_msg: %llu", write_frame_push_msg);
        MY_DEBUG("write_frame_pop_msg: %llu", write_frame_pop_msg);
        MY_DEBUG("write_allocation_msg: %llu", write_allocation_msg);
        MY_DEBUG("write_native_allocation_msg: %llu", write_native_allocation_msg);
        MY_DEBUG("write_pyrawframe_msg: %llu", write_pyrawframe_msg);
    }

    static void printMemoryrecordDebugCnt() {
        MY_DEBUG("write_memory_record_msg: %llu", write_memory_record_msg);
    }

    static void printProcessDebugCnt() {
        MY_DEBUG("total_processed_msg: %llu", total_processed_msg);
    }
    
    static void printReadDebugCnt()
    {
        MY_DEBUG("read_unresolvednativeframe_msg: %llu", read_unresolvednativeframe_msg);
        MY_DEBUG("add_cpu_sample: %llu", add_cpu_sample);
        MY_DEBUG("add_allocation: %llu", add_allocation);
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