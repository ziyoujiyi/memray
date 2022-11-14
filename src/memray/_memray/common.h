#ifndef _MEMRAY_COMMON_H
#define _MEMRAY_COMMON_H

#include "SPSCQueueOPT.h"
#include <iostream>
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

struct Timer
{
    // using namespace std::chrono_literals;
    std::chrono::time_point<std::chrono::steady_clock> start, end;
    // std::chrono::duration<float> duration;
    uint64_t duration;
    Timer()
    {
        //start = std::chrono::steady_clock::now();
    }

    void now() {
        start = std::chrono::steady_clock::now();
    }

    uint64_t elapsedNs()
    {
        end = std::chrono::steady_clock::now();
        duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        return duration;
    }

    uint64_t elapsedUs()
    {
        end = std::chrono::steady_clock::now();
        duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        return duration;
    }

    uint64_t elapsedMs()
    {
        end = std::chrono::steady_clock::now();
        duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        return duration;
    }

    uint64_t elapsedS()
    {
        end = std::chrono::steady_clock::now();
        duration = std::chrono::duration_cast<std::chrono::seconds>(end - start).count();
        return duration;
    }
    ~Timer()
    {
    }
};


class DebugInfo
{
public:
    static thread_local uint64_t track_memory_time;  // us
    static thread_local uint64_t track_cpu_time;
    static thread_local uint64_t backtrace_time;
    static thread_local uint64_t build_call_tree_time;
    static thread_local uint64_t dl_open_so_time;
    static thread_local uint64_t write_record_msg_time;
    static thread_local uint64_t write_threadspecific_record_msg_time;
    static thread_local uint64_t prepare_tracker_ins_time;
    static thread_local uint64_t proc_record_msg_time;

    static thread_local size_t total_used_msg_node;
    static thread_local size_t get_avaiable_msg_node_failed;
    static thread_local size_t total_processed_msg;

    static thread_local size_t blocked_cpu_sample;
    static thread_local size_t tracked_cpu_sample;
    static thread_local size_t blocked_allocation;
    static thread_local size_t tracked_native_allocation;
    static thread_local size_t tracked_allocation;

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
    #define MOD 1000000

    static void printTimeCost() {
        MY_DEBUG("*******************************************************");
        MY_DEBUG("track_memory_time(ms): %llu", track_memory_time / MOD);
        MY_DEBUG("track_cpu_time(ms): %llu", track_cpu_time / MOD);
        MY_DEBUG("backtrace_time(ms): %llu", backtrace_time / MOD);
        MY_DEBUG("build_call_tree_time(ms): %llu", build_call_tree_time / MOD);
        MY_DEBUG("dl_open_so_time(ms): %llu", dl_open_so_time / MOD);
        MY_DEBUG("write_record_msg_time(ms): %llu", write_record_msg_time / MOD);
        MY_DEBUG("write_threadspecific_record_msg_time(ms): %llu", write_threadspecific_record_msg_time / MOD);
        MY_DEBUG("prepare_tracker_ins_time(ms): %llu", prepare_tracker_ins_time / MOD);
    }

    static void printWriteDebugCnt()
    {
        MY_DEBUG("*******************************************************");
        MY_DEBUG("get_avaiable_msg_node_failed: %llu", get_avaiable_msg_node_failed);
        MY_DEBUG("total_used_msg_node: %llu", total_used_msg_node);
        MY_DEBUG("write_segment_header_msg: %llu", write_segment_msg);
        MY_DEBUG("write_segment_msg: %llu", write_segment_msg);
        MY_DEBUG("blocked_cpu_sample: %llu", blocked_cpu_sample);
        MY_DEBUG("tracked_cpu_sample: %llu", tracked_cpu_sample);
        MY_DEBUG("blocked_allocation: %llu", blocked_allocation);
        MY_DEBUG("tracked_native_allocation: %llu", tracked_native_allocation);
        MY_DEBUG("tracked_allocation: %llu", tracked_allocation);
        MY_DEBUG("write_native_allocation_msg: %llu", write_native_allocation_msg);
        MY_DEBUG("write_allocation_msg: %llu", write_allocation_msg);
        MY_DEBUG("write_unresolvednativeframe_msg: %llu", write_unresolvednativeframe_msg);
        MY_DEBUG("write_frame_push_msg: %llu", write_frame_push_msg);
        MY_DEBUG("write_frame_pop_msg: %llu", write_frame_pop_msg);
        MY_DEBUG("write_pyrawframe_msg: %llu", write_pyrawframe_msg);
    }

    static void printMemoryrecordDebugCnt() {
        MY_DEBUG("*******************************************************");
        MY_DEBUG("write_memory_record_msg: %llu", write_memory_record_msg);
    }

    static void printProcessDebugCnt() {
        MY_DEBUG("*******************************************************");
        MY_DEBUG("total_processed_msg: %llu", total_processed_msg);
        MY_DEBUG("proc_record_msg_time(ms): %llu", proc_record_msg_time / MOD);
    }
    
    static void printReadDebugCnt()
    {
        MY_DEBUG("*******************************************************");
        MY_DEBUG("read_unresolvednativeframe_msg: %llu", read_unresolvednativeframe_msg);
        MY_DEBUG("add_cpu_sample: %llu", add_cpu_sample);
        MY_DEBUG("add_allocation: %llu", add_allocation);
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