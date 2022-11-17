#ifndef _MEMRAY_COMMON_H
#define _MEMRAY_COMMON_H

#include "SPSCQueueOPT.h"
#include <assert.h>
#include <atomic>
#include <chrono>
#include <cstring>
#include <ctime>
#include <iostream>
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

#define PER_WRITE_PY_RAW_FRAMES_MAX 128

struct Timer
{
    // using namespace std::chrono_literals;
    std::chrono::time_point<std::chrono::steady_clock> start, end;
    // std::chrono::duration<float> duration;
    uint64_t duration;
    Timer()
    {
        // start = std::chrono::steady_clock::now();
    }

    void now()
    {
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
    static thread_local uint64_t track_memory_time;  // ns
    static thread_local uint64_t track_cpu_time;  // ns
    static thread_local uint64_t cpu_handler_time;
    static thread_local uint64_t backtrace_time;
    static thread_local uint64_t build_call_tree_time;
    static thread_local uint64_t dl_open_so_time;
    static thread_local uint64_t write_record_msg_time;
    static thread_local uint64_t write_threadspecific_record_msg_time;
    static thread_local uint64_t prepare_tracker_ins_time;
    static thread_local uint64_t proc_record_msg_time;

    static thread_local uint64_t total_used_msg_node;
    static thread_local uint64_t get_avaiable_msg_node_failed;
    static thread_local uint64_t total_processed_msg;

    static thread_local uint64_t blocked_cpu_sample_dueto_reading;
    static thread_local uint64_t blocked_cpu_sample_dueto_trackingmemory;
    static thread_local uint64_t tracked_cpu_sample;
    static thread_local uint64_t blocked_allocation;
    static thread_local uint64_t tracked_native_allocation;
    static thread_local uint64_t tracked_allocation;
    static thread_local uint64_t tracked_deallocation;

    static thread_local uint64_t write_unresolvednativeframe_msg;
    static thread_local uint64_t write_msg_with_context_time;
    static thread_local uint64_t write_frame_push_msg;
    static thread_local uint64_t write_frame_pop_msg;
    static thread_local uint64_t write_allocation_msg;
    static thread_local uint64_t write_native_allocation_msg;
    static thread_local uint64_t write_pyrawframe_msg;
    static thread_local uint64_t write_memory_record_msg;
    static thread_local uint64_t write_segment_header;
    static thread_local uint64_t write_segment;

    static thread_local uint64_t read_unresolvednativeframe_msg;

    static thread_local uint64_t add_cpu_sample;
    static thread_local uint64_t add_allocation;
#define MOD 1000000

    static void printTimeCost()
    {
        MY_DEBUG("*******************************************************");
        MY_DEBUG("track_memory_time(ms): %lu", track_memory_time / MOD);
        MY_DEBUG("track_cpu_time(ms): %lu", track_cpu_time / MOD);
        MY_DEBUG("cpu_handler_time(ms): %lu", cpu_handler_time / MOD);
        MY_DEBUG("backtrace_time(ms): %lu", backtrace_time / MOD);
        MY_DEBUG("build_call_tree_time(ms): %lu", build_call_tree_time / MOD);
        MY_DEBUG("dl_open_so_time(ms): %lu", dl_open_so_time / MOD);
        MY_DEBUG("write_record_msg_time(ms): %lu", write_record_msg_time / MOD);
        MY_DEBUG(
                "write_threadspecific_record_msg_time(ms): %lu",
                write_threadspecific_record_msg_time / MOD);
        MY_DEBUG("prepare_tracker_ins_time(ms): %lu", prepare_tracker_ins_time / MOD);
        MY_DEBUG("write_msg_with_context_time(ms): %lu", write_msg_with_context_time / MOD);
    }

    static void printWriteDebugCnt()
    {
        MY_DEBUG("*******************************************************");
        MY_DEBUG("get_avaiable_msg_node_failed: %lu", get_avaiable_msg_node_failed);
        MY_DEBUG("total_used_msg_node: %lu", total_used_msg_node);
        MY_DEBUG("write_segment_header: %lu", write_segment);
        MY_DEBUG("write_segment: %lu", write_segment);
        MY_DEBUG("blocked_cpu_sample_dueto_reading: %lu", blocked_cpu_sample_dueto_reading);
        MY_DEBUG(
                "blocked_cpu_sample_dueto_trackingmemory: %lu",
                blocked_cpu_sample_dueto_trackingmemory);
        MY_DEBUG("tracked_cpu_sample: %lu", tracked_cpu_sample);
        MY_DEBUG("blocked_allocation: %lu", blocked_allocation);
        MY_DEBUG("tracked_native_allocation: %lu", tracked_native_allocation);
        MY_DEBUG("tracked_allocation: %lu", tracked_allocation);
        MY_DEBUG("tracked_deallocation: %lu", tracked_deallocation);
        MY_DEBUG("write_native_allocation_msg: %lu", write_native_allocation_msg);
        MY_DEBUG("write_allocation_msg: %lu", write_allocation_msg);
        MY_DEBUG("write_unresolvednativeframe_msg: %lu", write_unresolvednativeframe_msg);
        MY_DEBUG("write_frame_push_msg: %lu", write_frame_push_msg);
        MY_DEBUG("write_frame_pop_msg: %lu", write_frame_pop_msg);
        MY_DEBUG("write_pyrawframe_msg: %lu", write_pyrawframe_msg);
    }

    static void printMemoryrecordDebugCnt()
    {
        MY_DEBUG("*******************************************************");
        MY_DEBUG("write_memory_record_msg: %lu", write_memory_record_msg);
    }

    static void printProcessDebugCnt()
    {
        MY_DEBUG("*******************************************************");
        MY_DEBUG("total_processed_msg: %lu", total_processed_msg);
        MY_DEBUG("proc_record_msg_time(ms): %lu", proc_record_msg_time / MOD);
    }

    static void printReadDebugCnt()
    {
        MY_DEBUG("*******************************************************");
        MY_DEBUG("read_unresolvednativeframe_msg: %lu", read_unresolvednativeframe_msg);
        MY_DEBUG("add_cpu_sample: %lu", add_cpu_sample);
        MY_DEBUG("add_allocation: %lu", add_allocation);
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

class PythonStackGuard
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

bool inline copyChar(char* dst, const char* src)
{
    /*int src_len = strlen(src) + 1;
    int sz = sizeof(dst) / sizeof(dst[0]);
    assert(src_len <= sz);*/
    strcpy(dst, src);  // notice: possible overflow!
    return true;
}
}  // namespace memray
#endif  //_MEMRAY_COMMON_H