#pragma once

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <thread>
#include <unordered_set>

#include <unwind.h>

#include "frame_tree.h"
#include "hooks.h"
#include "linker_shenanigans.h"
#include "record_writer.h"
#include "records.h"

#if defined(USE_MEMRAY_TLS_MODEL)
#    if defined(__GLIBC__)
#        define MEMRAY_FAST_TLS __attribute__((tls_model("initial-exec")))
#    else
#        define MEMRAY_FAST_TLS __attribute__((tls_model("local-dynamic")))
#    endif
#else
#    define MEMRAY_FAST_TLS
#endif

namespace memray::tracking_api {

struct RecursionGuard
{
    RecursionGuard()
    : wasLocked(isActive)
    {
        isActive = true;
    }

    ~RecursionGuard()
    {
        isActive = wasLocked;
    }

    const bool wasLocked;
    MEMRAY_FAST_TLS static thread_local bool isActive;
};


template <typename T>
class NewSTLAllocator : public std::allocator<T> {
public:
    template <typename U>
    struct rebind {
        typedef NewSTLAllocator<U> other;
    };

public:
    T* allocate(size_t n, const void* hint = 0) {
        return (T*)hooks::malloc(n * sizeof(T));
    }

    void deallocate(T* ptr, size_t n) {
        hooks::free(ptr);
    }
};


// Trace function interface

/**
 * Trace function to be installed in all Python treads to track function calls
 *
 * This trace function's sole purpose is to give a thread-safe, GIL-synchronized view of the Python
 * stack. To retrieve the Python stack using the C-API forces the caller to have the GIL held. Requiring
 * the GIL in the allocator function has too much impact on performance and can deadlock extension
 * modules that have native locks that are not synchronized themselves with the GIL. For this reason we
 * need a way to record and store the Python call frame information in a way that we can read without the
 * need to use the C-API. This trace function writes to disk the PUSH and POP operations so the Python
 *stack at any point can be reconstructed later.
 *
 **/
int
PyTraceFunction(PyObject* obj, PyFrameObject* frame, int what, PyObject* arg);

/**
 * Trampoline that serves as the initial profiling function for each thread.
 *
 * This performs some one-time setup, then installs PyTraceFunction.
 */
int
PyTraceTrampoline(PyObject* obj, PyFrameObject* frame, int what, PyObject* arg);

/**
 * Installs the trace function in the current thread.
 *
 * This function installs the trace function in the current thread using the C-API.
 *
 * */
void
install_trace_function();

/**
 * Drop any references to frames on this thread's stack.
 *
 * This should be called when either the thread is dying or our profile
 * function is being uninstalled from it.
 */
void
forget_python_stack();

/**
 * Sets a flag to enable integration with the `greenlet` module.
 */
void
begin_tracking_greenlets();

/**
 * Handle a notification of control switching from one greenlet to another.
 */
void
handle_greenlet_switch(PyObject* from, PyObject* to);


/**
 * Singleton managing all the global state and functionality of the tracing mechanism
 *
 * This class acts as the only interface to the tracing functionality and encapsulates all the
 * required global state. *All access* must be done through the singleton interface as the singleton
 * has the same lifetime of the entire program. The singleton can be activated and deactivated to
 * temporarily stop the tracking as desired. The singleton manages a mirror copy of the Python stack
 * so it can be accessed synchronized by its the allocation tracking interfaces.
 * */
class Tracker
{
  public:
    // Constructors
    ~Tracker();

    Tracker(Tracker& other) = delete;
    Tracker(Tracker&& other) = delete;
    void operator=(const Tracker&) = delete;
    void operator=(Tracker&&) = delete;

    // Interface to get the tracker instance
    static PyObject* createTracker(
            std::unique_ptr<RecordWriter> record_writer,
            bool native_traces,
            unsigned int memory_interval,
            bool follow_fork,
            bool trace_python_allocators);
    static PyObject* destroyTracker();
    static Tracker* getTracker();

    // Cpu tracking interface
    __attribute__((always_inline)) inline static void
    trackCpu(int signo)
    {
        //static size_t num = 0; 
        switch(signo) {
            case SIGALRM:
                Tracker* tracker = getTracker();
                if (tracker) {
                    tracker->trackCpuImpl(hooks::Allocator::CPU_SAMPLING);
                }
                break;
        }
        //if (++num == 10000) {
        //    kill(getpid(), SIGKILL);
        //}
    }

    // Allocation tracking interface
    __attribute__((always_inline)) inline static void
    trackAllocation(void* ptr, size_t size, hooks::Allocator func)
    {
        Tracker* tracker = getTracker();
        if (tracker) {
            tracker->trackAllocationImpl(ptr, size, func);
        }
    }

    __attribute__((always_inline)) inline static void
    trackDeallocation(void* ptr, size_t size, hooks::Allocator func)
    {
        Tracker* tracker = getTracker();
        if (tracker) {
            tracker->trackDeallocationImpl(ptr, size, func);
        }
    }

    __attribute__((always_inline)) inline static void invalidate_module_cache()
    {
        Tracker* tracker = getTracker();
        if (tracker) {
            tracker->invalidate_module_cache_impl();
        }
    }

    __attribute__((always_inline)) inline static void updateModuleCache()
    {
        Tracker* tracker = getTracker();
        if (tracker) {
            tracker->updateModuleCacheImpl();
        }
    }

    __attribute__((always_inline)) inline static void registerThreadName(const char* name)
    {
        Tracker* tracker = getTracker();
        if (tracker) {
            tracker->registerThreadNameImpl(name);
        }
    }

    // RawFrame stack interface
    bool pushFrame(const RawFrame& frame);
    bool popFrames(uint32_t count);

    // Interface to activate/deactivate the tracking
    static const std::atomic<bool>& isActive();
    static void activate();
    static void deactivate();

  private:
    class BackgroundThread
    {
      public:
        // Constructors
        BackgroundThread(std::shared_ptr<RecordWriter> record_writer, unsigned int memory_interval);

        // Methods
        void start();
        void stop();

      private:
        // Data members
        std::shared_ptr<RecordWriter> d_writer;
        bool d_stop{false};
        std::atomic_int d_cpu_sampling_cnt{0};
        unsigned int d_cpu_to_memory_sampling_ratio{1};  // default value
        unsigned int d_cpu_profiler_interval;
        unsigned int d_memory_interval;
        std::mutex d_mutex;
        std::condition_variable d_cv;
        std::thread d_thread;
        mutable std::ifstream d_procs_statm;

        // Methods
        size_t getRSS() const;
        static unsigned long int timeElapsed();
    };

    // Data members
    FrameCollection<RawFrame> d_frames;
    static std::atomic<bool> d_active;
    static std::unique_ptr<Tracker> d_instance_owner;
    static std::atomic<Tracker*> d_instance;

    std::shared_ptr<RecordWriter> d_writer;
    FrameTree d_native_trace_tree;
    bool d_unwind_native_frames;
    unsigned int d_memory_interval;
    bool d_follow_fork;
    bool d_trace_python_allocators;
    linker::SymbolPatcher d_patcher;
    std::unique_ptr<BackgroundThread> d_background_thread;

    // Methods
    frame_id_t registerFrame(const RawFrame& frame);

    void trackCpuImpl(hooks::Allocator func);
    void trackAllocationImpl(void* ptr, size_t size, hooks::Allocator func);
    void trackDeallocationImpl(void* ptr, size_t size, hooks::Allocator func);
    static inline void startTrace() {
        hooks::MEMORY_TRACE_SWITCH = true;
    }
    static inline void stopTrace() {
        hooks::MEMORY_TRACE_SWITCH = false;
    }
    void invalidate_module_cache_impl();
    void updateModuleCacheImpl();
    void registerThreadNameImpl(const char* name);
    void registerPymallocHooks() const noexcept;
    void unregisterPymallocHooks() const noexcept;

    explicit Tracker(
            std::unique_ptr<RecordWriter> record_writer,
            bool native_traces,
            unsigned int memory_interval,
            bool follow_fork,
            bool trace_python_allocators);

    static void prepareFork();
    static void parentFork();
    static void childFork();
};

}  // namespace memray::tracking_api
