#pragma once
#include <assert.h>
#include <iostream>
#include <mutex>
#include <vector>

#include "records.h"

#include "frameobject.h"

#if defined(__linux__)
#    define UNW_LOCAL_ONLY
#    include <libunwind.h>
#elif defined(__APPLE__)
#    include <execinfo.h>
#endif

namespace memray::tracking_api {

#define NATIVE_TRACE_MAX_SIZE 1024
#define MAX_STROE_BACKTRACE 1
#define BACKTRACE_SKIP 2

using ip_t = frame_id_t;

class BackTraceData
{
  public:
    static BackTraceData& getInstance()
    {
        static BackTraceData ins;
        return ins;
    }

  private:
    void Init()
    {
        mem_trace_cnt_ptr = std::make_shared<size_t>(0);
        d_mem_sizes_ptr = std::make_shared<std::vector<ip_t>>(MAX_STROE_BACKTRACE, 0);
        d_mem_data_ptr =
                std::make_shared<std::vector<ip_t>>(NATIVE_TRACE_MAX_SIZE * MAX_STROE_BACKTRACE);

        cpu_trace_cnt_ptr = std::make_shared<size_t>(0);
        d_cpu_sizes_ptr = std::make_shared<std::vector<ip_t>>(MAX_STROE_BACKTRACE, 0);
        d_cpu_data_ptr =
                std::make_shared<std::vector<ip_t>>(NATIVE_TRACE_MAX_SIZE * MAX_STROE_BACKTRACE);
    }

  public:
    std::shared_ptr<size_t> mem_trace_cnt_ptr;
    std::shared_ptr<std::vector<ip_t>> d_mem_sizes_ptr;
    std::shared_ptr<std::vector<ip_t>> d_mem_data_ptr;

    std::shared_ptr<size_t> cpu_trace_cnt_ptr;
    std::shared_ptr<std::vector<ip_t>> d_cpu_sizes_ptr;
    std::shared_ptr<std::vector<ip_t>> d_cpu_data_ptr;

  private:
    BackTraceData()
    {
        Init();
    }
    ~BackTraceData()
    {
    }
    BackTraceData& operator=(const BackTraceData&);
    BackTraceData(const BackTraceData&);
};

static BackTraceData* single = &BackTraceData::getInstance();

class NativeTrace
{
  public:
    static NativeTrace& getInstance(int flag)
    {
        if (flag == 0) {
            static NativeTrace mem_ins(0);
            return mem_ins;
        } else {
            static NativeTrace cpu_ins(1);
            return cpu_ins;
        }
    }

  public:
    std::shared_ptr<size_t> d_cnt_ptr;
    std::shared_ptr<std::vector<ip_t>> d_sizes_ptr;
    std::shared_ptr<std::vector<ip_t>> d_data_ptr;
    std::int32_t d_skip{0};

  private:
    NativeTrace(int flag)
    {
        if (flag == 0) {
            d_cnt_ptr = single->mem_trace_cnt_ptr;
            d_sizes_ptr = single->d_mem_sizes_ptr;
            d_data_ptr = single->d_mem_data_ptr;
        } else {
            d_cnt_ptr = single->cpu_trace_cnt_ptr;
            d_sizes_ptr = single->d_cpu_sizes_ptr;
            d_data_ptr = single->d_cpu_data_ptr;
        }
    }
    NativeTrace& operator=(const NativeTrace&);
    NativeTrace(const NativeTrace&);
    ~NativeTrace()
    {
    }

  public:
    enum WRITE_READ_FLAG { WRITE_ONLY = 0, READ_ONLY = 1 };

    static int32_t write_read_flag;
    thread_id_t backtrace_thread_id = 0;

  public:
    __attribute__((always_inline)) inline bool fill()
    {
        Timer t;
        t.now();
        size_t size = unw_backtrace(
                (void**)d_data_ptr->data(),
                NATIVE_TRACE_MAX_SIZE);  // https://github.com/dropbox/libunwind/blob/master/doc/unw_backtrace.man
                                         // https://github.com/dropbox/libunwind/blob/16bf4e5e498c7fc528256843a4a724edc2753ffd/src/x86_64/Gtrace.c
        if (likely(size > BACKTRACE_SKIP)) {
            (*d_sizes_ptr)[*d_cnt_ptr] = size - BACKTRACE_SKIP;
            //(*d_cnt_ptr)++;
            d_skip = BACKTRACE_SKIP;
            DebugInfo::backtrace_time += t.elapsedNs();
            return true;
        } else {
            (*d_sizes_ptr)[*d_cnt_ptr] = 0;
            DebugInfo::backtrace_time += t.elapsedNs();
            return false;
        }
    }

    static void setup()
    {
#ifdef __linux__
        // configure libunwind for better speed
        if (unw_set_caching_policy(unw_local_addr_space, UNW_CACHE_PER_THREAD)) {
            fprintf(stderr, "WARNING: Failed to enable per-thread libunwind caching.\n");
        }
#    if (UNW_VERSION_MAJOR > 1 && UNW_VERSION_MINOR >= 3)
        if (unw_set_cache_size(unw_local_addr_space, 1024, 0)) {
            fprintf(stderr, "WARNING: Failed to set libunwind cache size.\n");
        }
#    endif
#else
        return;
#endif
    }

    static inline void flushCache()
    {
#ifdef __linux__
        unw_flush_cache(unw_local_addr_space, 0, 0);
#else
        return;
#endif
    }
};

class FrameTree
{
  public:
    using index_t = uint32_t;

    inline std::pair<frame_id_t, index_t> nextNode(index_t index) const
    {
        std::lock_guard<std::mutex> lock(d_mutex);
        assert(1 <= index && index <= d_graph.size());
        return std::make_pair(d_graph[index].frame_id, d_graph[index].parent_index);
    }

    using tracecallback_t = std::function<bool(frame_id_t, index_t)>;

    template<typename T>
    size_t getTraceIndex(const T* stack_trace, const tracecallback_t& callback)
    {
        std::lock_guard<std::mutex> lock(d_mutex);
        // Timer t;
        // t.now();
        index_t index = 0;
        int64_t backtrace_idx = 0;
        int64_t size = stack_trace->d_sizes_ptr->at(backtrace_idx);
        int64_t start = NATIVE_TRACE_MAX_SIZE * backtrace_idx;
        int32_t skip = stack_trace->d_skip;
        for (int64_t i = start + size - 1; i >= (start + skip); i--) {
            frame_id_t frame = stack_trace->d_data_ptr->at(i);
            index = getTraceIndexUnsafe(index, frame, callback);
        }
        // DebugInfo::build_call_tree_time += t.elapsedNs();
        return index;
    }

    size_t getTraceIndex(index_t parent_index, frame_id_t frame)
    {
        std::lock_guard<std::mutex> lock(d_mutex);
        return getTraceIndexUnsafe(parent_index, frame, tracecallback_t());
    }

  private:
    size_t getTraceIndexUnsafe(index_t parent_index, frame_id_t frame, const tracecallback_t& callback)
    {
        Node& parent = d_graph[parent_index];
        auto it = std::lower_bound(parent.children.begin(), parent.children.end(), frame);
        if (it == parent.children.end() || it->frame_id != frame) {
            index_t new_index = d_graph.size();
            it = parent.children.insert(it, {frame, new_index});
            if (callback && !callback(frame, parent_index)) {
                return 0;
            }
            d_graph.push_back({frame, parent_index});
        }
        return it->child_index;
    }

    struct DescendentEdge
    {
        frame_id_t frame_id;
        index_t child_index;

        bool operator<(frame_id_t frame_id) const
        {
            return this->frame_id < frame_id;
        }
    };

    struct Node
    {
        frame_id_t frame_id;
        index_t parent_index;
        std::vector<DescendentEdge> children;
    };
    mutable std::mutex d_mutex;
    mutable SpinMutex d_spin_mutex;
    std::vector<Node> d_graph{{0, 0, {}}};
};
}  // namespace memray::tracking_api
