#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <cassert>
#include <limits.h>

#ifdef __linux__
#    include <link.h>
#elif defined(__APPLE__)
#    include "macho_utils.h"
#    include <mach/mach.h>
#    include <mach/task.h>
#endif

#include <algorithm>
#include <iostream>
#include <mutex>
#include <type_traits>
#include <unistd.h>
#include <utility>

#include "compat.h"
#include "exceptions.h"
#include "hooks.h"
#include "record_writer.h"
#include "records.h"
#include "tracking_api.h"
#include "common.h"

using namespace memray::exception;
using namespace std::chrono_literals;

namespace {

#ifdef __linux__
std::string
get_executable()
{
    char buff[PATH_MAX + 1];
    ssize_t len = ::readlink("/proc/self/exe", buff, sizeof(buff));
    if (len > PATH_MAX) {
        throw std::runtime_error("Path to executable is more than PATH_MAX bytes");
    } else if (len == -1) {
        throw std::runtime_error("Could not determine executable path");
    }
    return std::string(buff, len);
}

static bool
starts_with(const std::string& haystack, const std::string_view& needle)
{
    return haystack.compare(0, needle.size(), needle) == 0;
}
#endif

}  // namespace

namespace memray::tracking_api {

MEMRAY_FAST_TLS thread_local bool RecursionGuard::isActive = false;

static inline thread_id_t
thread_id()
{
    return reinterpret_cast<thread_id_t>(pthread_self());
};

// Tracker interface

// This class must have a trivial destructor (and therefore all its instance
// attributes must be POD). This is required because the libc implementation
// can perform allocations even after the thread local variables for a thread
// have been destroyed. If a TLS variable that is not trivially destructable is
// accessed after that point by our allocation hooks, it will be resurrected,
// and then it will be freed when the thread dies, but its destructor either
// won't be called at all, or will be called on freed memory when some
// arbitrary future thread is destroyed (if the pthread struct is reused for
// another thread).
class PythonStackTracker
{
  private:
    PythonStackTracker()
    {
    }

    enum class FrameState {
        NOT_EMITTED = 0,
        EMITTED_BUT_LINE_NUMBER_MAY_HAVE_CHANGED = 1,
        EMITTED_AND_LINE_NUMBER_HAS_NOT_CHANGED = 2,
    };

    struct LazilyEmittedFrame
    {
        PyFrameObject* frame;
        RawFrame raw_frame_record;
        FrameState state;
    };

  public:
    static bool s_greenlet_tracking_enabled;
    static bool s_native_tracking_enabled;

    static void installProfileHooks();
    static void removeProfileHooks();

    void clear();

    static PythonStackTracker& get();
    void emitPendingPushesAndPops();
    void invalidateMostRecentFrameLineNumber();
    int pushPythonFrame(PyFrameObject* frame);
    void popPythonFrame();

    void installGreenletTraceFunctionIfNeeded();
    void handleGreenletSwitch(PyObject* from, PyObject* to);

  private:
    // Fetch the thread-local stack tracker without checking if its stack needs to be reloaded.
    static PythonStackTracker& getUnsafe();

    static std::vector<LazilyEmittedFrame> pythonFrameToStack(PyFrameObject* current_frame);
    static void recordAllStacks();
    void reloadStackIfTrackerChanged();

    void pushLazilyEmittedFrame(const LazilyEmittedFrame& frame);

    static std::mutex s_mutex;
    static std::unordered_map<PyThreadState*, std::vector<LazilyEmittedFrame>> s_initial_stack_by_thread;
    static std::atomic<unsigned int> s_tracker_generation;

    uint32_t d_num_pending_pops{};
    uint32_t d_tracker_generation{};
    std::vector<LazilyEmittedFrame>* d_stack{};
    bool d_greenlet_hooks_installed{};
};

bool PythonStackTracker::s_greenlet_tracking_enabled{false};
bool PythonStackTracker::s_native_tracking_enabled{false};

std::mutex PythonStackTracker::s_mutex;
std::unordered_map<PyThreadState*, std::vector<PythonStackTracker::LazilyEmittedFrame>>
        PythonStackTracker::s_initial_stack_by_thread;
std::atomic<unsigned int> PythonStackTracker::s_tracker_generation;

PythonStackTracker&
PythonStackTracker::get()
{
    PythonStackTracker& ret = getUnsafe();
    ret.reloadStackIfTrackerChanged();
    return ret;
}

PythonStackTracker&
PythonStackTracker::getUnsafe()
{
    // See giant comment above.
    static_assert(std::is_trivially_destructible<PythonStackTracker>::value);
    MEMRAY_FAST_TLS thread_local PythonStackTracker t_python_stack_tracker;  // static type
    return t_python_stack_tracker;
}

void
PythonStackTracker::emitPendingPushesAndPops()
{
    if (!d_stack) {
        return;
    }

    // At any time, the stack contains (in this order):
    // - Any number of EMITTED_AND_LINE_NUMBER_HAS_NOT_CHANGED frames
    // - 0 or 1 EMITTED_BUT_LINE_NUMBER_MAY_HAVE_CHANGED frame
    // - Any number of NOT_EMITTED frames
    //MY_DEBUG("entering emitPendingPushesAndPops >>>");
    auto it = d_stack->rbegin();
    for (; it != d_stack->rend(); ++it) {
        if (it->state == FrameState::NOT_EMITTED) {
            it->raw_frame_record.lineno = PyFrame_GetLineNumber(it->frame);
        } else if (it->state == FrameState::EMITTED_BUT_LINE_NUMBER_MAY_HAVE_CHANGED) {
            int lineno = PyFrame_GetLineNumber(it->frame);
            if (lineno != it->raw_frame_record.lineno) {
                // Line number was wrong; emit an artificial pop so we can push
                // back in with the right line number.
                d_num_pending_pops++;
                it->state = FrameState::NOT_EMITTED;
                it->raw_frame_record.lineno = lineno;
            } else {
                it->state = FrameState::EMITTED_AND_LINE_NUMBER_HAS_NOT_CHANGED;
                break;
            }
        } else {
            assert(it->state == FrameState::EMITTED_AND_LINE_NUMBER_HAS_NOT_CHANGED);
            break;
        }
    }
    auto first_to_emit = it.base();
    size_t sz = d_stack->size();
    // Emit pending pops
    Tracker::getTracker()->popFrames(d_num_pending_pops);
    d_num_pending_pops = 0;

    // Emit pending pushes
    for (auto to_emit = first_to_emit; to_emit != d_stack->end(); ++to_emit) {
        if (!Tracker::getTracker()->pushFrame(to_emit->raw_frame_record)) {
            break;
        }
        to_emit->state = FrameState::EMITTED_AND_LINE_NUMBER_HAS_NOT_CHANGED;
    }

    invalidateMostRecentFrameLineNumber();
}

void
PythonStackTracker::invalidateMostRecentFrameLineNumber()
{
    // As bytecode instructions are executed, the line number in the most
    // recent Python frame can change without us finding out. Cache its line
    // number, but verify it the next time this frame might need to be emitted.
    if (!d_stack->empty()
        && d_stack->back().state == FrameState::EMITTED_AND_LINE_NUMBER_HAS_NOT_CHANGED) {
        d_stack->back().state = FrameState::EMITTED_BUT_LINE_NUMBER_MAY_HAVE_CHANGED;
    }
}

void
PythonStackTracker::reloadStackIfTrackerChanged()
{
    // Note: this function does not require the GIL.
    if (d_tracker_generation == s_tracker_generation) {
        return;
    }

    // If we reach this point, a new Tracker was installed by another thread,
    // which also captured our Python stack. Trust it, ignoring any stack we
    // already hold (since the stack we hold could be incorrect if tracking
    // stopped and later restarted underneath our still-running thread).

    if (d_stack) {
        d_stack->clear();
    }
    d_num_pending_pops = 0;

    std::vector<LazilyEmittedFrame> correct_stack;

    {
        std::unique_lock<std::mutex> lock(s_mutex);
        d_tracker_generation = s_tracker_generation;

        auto it = s_initial_stack_by_thread.find(PyGILState_GetThisThreadState());
        if (it != s_initial_stack_by_thread.end()) {
            it->second.swap(correct_stack);
            s_initial_stack_by_thread.erase(it);
        }
    }

    // Iterate in reverse so that we push the most recent call last
    for (auto frame_it = correct_stack.rbegin(); frame_it != correct_stack.rend(); ++frame_it) {
        pushLazilyEmittedFrame(*frame_it);
    }
    // emitPendingPushesAndPops();  // not work ????
}

int
PythonStackTracker::pushPythonFrame(PyFrameObject* frame)
{
    //MY_DEBUG("entering pushPythonFrame >>> ");
    installGreenletTraceFunctionIfNeeded();

    PyCodeObject* code = compat::frameGetCode(frame);
    const char* function = PyUnicode_AsUTF8(code->co_name);
    if (function == nullptr) {
        return -1;
    }

    const char* filename = PyUnicode_AsUTF8(code->co_filename);
    if (filename == nullptr) {
        return -1;
    }

    // If native tracking is not enabled, treat every frame as an entry frame.
    // It doesn't matter to the reader, and is more efficient.
    bool is_entry_frame = !s_native_tracking_enabled || compat::isEntryFrame(frame);
    pushLazilyEmittedFrame({frame, {function, filename, 0, is_entry_frame}, FrameState::NOT_EMITTED});
    emitPendingPushesAndPops();
    return 0;
}

void
PythonStackTracker::pushLazilyEmittedFrame(const LazilyEmittedFrame& frame)
{
    // Note: this function does not require the GIL.
    if (!d_stack) {
        d_stack = new std::vector<LazilyEmittedFrame>;
        d_stack->reserve(1024);
    }
    d_stack->push_back(frame);
}

void
PythonStackTracker::popPythonFrame()
{
    installGreenletTraceFunctionIfNeeded();

    if (!d_stack || d_stack->empty()) {
        return;
    }

    if (d_stack->back().state != FrameState::NOT_EMITTED) {
        d_num_pending_pops += 1;
        assert(d_num_pending_pops != 0);  // Ensure we didn't overflow.
    }
    d_stack->pop_back();
    invalidateMostRecentFrameLineNumber();
    emitPendingPushesAndPops();
}

void
PythonStackTracker::installGreenletTraceFunctionIfNeeded()
{
    if (!s_greenlet_tracking_enabled || d_greenlet_hooks_installed) {
        return;  // Nothing to do.
    }

    assert(PyGILState_Check());

    RecursionGuard guard;

    // Borrowed reference
    PyObject* modules = PySys_GetObject("modules");
    if (!modules) {
        return;
    }

    // Borrowed reference
    // Look directly at `sys.modules` since we only want to do something if
    // `greenlet._greenlet` has already been imported.
    PyObject* _greenlet = PyDict_GetItemString(modules, "greenlet._greenlet");
    if (!_greenlet) {
        return;
    }

    // Borrowed reference
    PyObject* _memray = PyDict_GetItemString(modules, "memray._memray");
    if (!_memray) {
        return;
    }

    PyObject* ret = PyObject_CallMethod(
            _greenlet,
            "settrace",
            "N",
            PyObject_GetAttrString(_memray, "greenlet_trace_function"));
    Py_XDECREF(ret);

    if (!ret) {
        // This might be hit from PyGILState_Ensure when a new thread state is
        // created on a C thread, so we can't reasonably raise the exception.
        PyErr_Print();
        _exit(1);
    }

    // Note: guarded by the GIL
    d_greenlet_hooks_installed = true;

    static bool warned = false;
    if (!warned) {
        warned = true;

        PyObject* ret = PyObject_CallMethod(_memray, "print_greenlet_warning", nullptr);
        Py_XDECREF(ret);
        if (!ret) {
            PyErr_Print();
            _exit(1);
        }
    }
}

void
PythonStackTracker::handleGreenletSwitch(PyObject*, PyObject*)
{
    RecursionGuard guard;

    // Clear any old TLS stack, emitting pops for frames that had been pushed.
    if (d_stack) {
        d_num_pending_pops += std::count_if(d_stack->begin(), d_stack->end(), [](const auto& f) {
            return f.state != FrameState::NOT_EMITTED;
        });
        d_stack->clear();
        emitPendingPushesAndPops();
    }

    // Re-create our TLS stack from our Python frames, most recent last.
    // Note: `frame` may be null; the new greenlet may not have a Python stack.
    PyFrameObject* frame = PyEval_GetFrame();

    std::vector<PyFrameObject*> stack;
    while (frame) {
        stack.push_back(frame);
        frame = compat::frameGetBack(frame);
    }

    std::for_each(stack.rbegin(), stack.rend(), [this](auto& frame) { pushPythonFrame(frame); });
}

std::atomic<bool> Tracker::d_active = false;
std::unique_ptr<Tracker> Tracker::d_instance_owner;
std::atomic<Tracker*> Tracker::d_instance = nullptr;

std::vector<PythonStackTracker::LazilyEmittedFrame>
PythonStackTracker::pythonFrameToStack(PyFrameObject* current_frame)
{
    std::vector<LazilyEmittedFrame> stack;

    while (current_frame) {
        PyCodeObject* code = compat::frameGetCode(current_frame);

        const char* function = PyUnicode_AsUTF8(code->co_name);
        if (function == nullptr) {
            return {};
        }

        const char* filename = PyUnicode_AsUTF8(code->co_filename);
        if (filename == nullptr) {
            return {};
        }

        stack.push_back({current_frame, {function, filename, 0}, FrameState::NOT_EMITTED});
        current_frame = compat::frameGetBack(current_frame);
    }

    return stack;
}

void
PythonStackTracker::recordAllStacks()
{
    assert(PyGILState_Check());

    // Record the current Python stack of every thread
    std::unordered_map<PyThreadState*, std::vector<LazilyEmittedFrame>> stack_by_thread;
    for (PyThreadState* tstate =
                 PyInterpreterState_ThreadHead(compat::threadStateGetInterpreter(PyThreadState_Get()));
         tstate != nullptr;
         tstate = PyThreadState_Next(tstate))
    {
        PyFrameObject* frame = compat::threadStateGetFrame(tstate);
        if (!frame) {
            continue;
        }

        stack_by_thread[tstate] = pythonFrameToStack(frame);
        if (PyErr_Occurred()) {
            throw std::runtime_error("Failed to capture a thread's Python stack");
        }
    }

    // Throw away all but the most recent frame for this thread.
    // We ignore every stack frame above `Tracker.__enter__`.
    PyThreadState* tstate = PyThreadState_Get();
    assert(stack_by_thread[tstate].size() >= 1);
    stack_by_thread[tstate].resize(1);

    std::unique_lock<std::mutex> lock(s_mutex);
    s_initial_stack_by_thread.swap(stack_by_thread);

    // Register that tracking has begun (again?), telling threads to sync their
    // TLS from these captured stacks. Update this atomically with the map, or
    // a thread that's 2 generations behind could grab the new stacks with the
    // previous generation number and immediately think they're out of date.
    s_tracker_generation++;
}

void
PythonStackTracker::installProfileHooks()
{
    assert(PyGILState_Check());

    // Uninstall any existing profile function in all threads. Do this before
    // installing ours, since we could lose the GIL if the existing profile arg
    // has a __del__ that gets called. We must hold the GIL for the entire time
    // we capture threads' stacks and install our trace function into them, so
    // their stacks can't change after we've captured them and before we've
    // installed our profile function that utilizes the captured stacks, and so
    // they can't start profiling before we capture their stack and miss it.
    compat::setprofileAllThreads(nullptr, nullptr);

    // Find and record the Python stack for all existing threads.
    recordAllStacks();

    // Install our profile trampoline in all existing threads.
    compat::setprofileAllThreads(PyTraceTrampoline, nullptr);
}

void
PythonStackTracker::removeProfileHooks()
{
    assert(PyGILState_Check());
    compat::setprofileAllThreads(nullptr, nullptr);
    std::unique_lock<std::mutex> lock(s_mutex);
    s_initial_stack_by_thread.clear();
}

void
PythonStackTracker::clear()
{
    if (!d_stack) {
        return;
    }

    while (!d_stack->empty()) {
        d_num_pending_pops += (d_stack->back().state != FrameState::NOT_EMITTED);
        d_stack->pop_back();
    }
    emitPendingPushesAndPops();
    delete d_stack;
    d_stack = nullptr;
}

Tracker::Tracker(
        std::unique_ptr<RecordWriter> record_writer,
        std::unique_ptr<RecordWriter> other_writer,
        bool native_traces,
        unsigned int memory_interval,
        bool follow_fork,
        bool trace_python_allocators)
: d_writer(std::move(record_writer))
, d_other_writer(std::move(other_writer))
, d_unwind_native_frames(native_traces)
, d_memory_interval(memory_interval)
, d_follow_fork(follow_fork)
, d_trace_python_allocators(trace_python_allocators)
{
    MY_DEBUG("main thread id: %lld ", thread_id());
	// Note: this must be set before the hooks are installed.
    d_instance = this;
    unsigned int cpu_interval = 11;  // ms

    static std::once_flag once;
    call_once(once, [] {
        hooks::ensureAllHooksAreValid();
        NativeTrace::setup();
        // We must do this last so that a child can't inherit an environment
        // where only half of our one-time setup is done.
        pthread_atfork(&prepareFork, &parentFork, &childFork);
    });

    if (!d_writer->writeHeader(false)) {
        throw IoError{"Failed to write output header"};
    }

    // d_writer->writeHeaderMsg(false);

    updateModuleCache();

    RecursionGuard guard;

    PythonStackTracker::s_native_tracking_enabled = native_traces;
    PythonStackTracker::installProfileHooks();
    if (d_trace_python_allocators) {
        registerPymallocHooks();
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = trackCpu;
    //  sigaddset(&sa.sa_mask,SIGQUIT);
    //  sigaddset(&sa.sa_mask,SIGTERM)
    //  sa.sa_flags = SA_NODEFER | SA_RESETHAND;
    //  sa.sa_flags = 0;

    if (sigaction(SIGALRM, &sa, nullptr) < 0) {
        LOG(ERROR) << "sigaction error";
    }
    // auto old_handler = signal(SIGALRM, trackCpu);
    // assert(old_handler == SIG_DFL);
    // signal(SIGALRM, old_handler);

    MY_DEBUG("cpu_interval: %lld ms", cpu_interval);
    MY_DEBUG("memory_interval: %d ms", memory_interval);

    struct itimerval tick, oldTick;
    memset(&tick, 0, sizeof(tick));
    tick.it_value.tv_sec = 0;
    tick.it_value.tv_usec = 10000;
    tick.it_interval.tv_sec = 0;
    tick.it_interval.tv_usec = cpu_interval * 1000;
    int ret = setitimer(ITIMER_REAL, &tick, &oldTick);
    if (ret) {
        LOG(ERROR) << "set timer failed: " << ret;
    }
    MY_DEBUG("tracker is not actived!");
    d_background_thread =
            std::make_unique<BackgroundThread>(d_writer, d_other_writer, memory_interval, cpu_interval);
    d_background_thread->start();
    d_background_thread->startWriteRecord();

    d_patcher.overwrite_symbols();
    tracking_api::Tracker::activate();
}

Tracker::~Tracker()
{
    RecursionGuard guard;
    tracking_api::Tracker::deactivate();
    PythonStackTracker::s_native_tracking_enabled = false;
    d_background_thread->stop();
    d_background_thread->stopWriteRecord();
    d_patcher.restore_symbols();
    if (d_trace_python_allocators) {
        unregisterPymallocHooks();
    }
    PythonStackTracker::removeProfileHooks();

    d_writer->writeTrailer();
    d_writer->writeHeader(true);

    // d_writer->writeTrailerMsg();
    // d_writer->writeHeaderMsg(true);

    d_writer.reset();

    // Note: this must not be unset before the hooks are uninstalled.
    d_instance = nullptr;

    DebugInfo::printWriteDebugCnt();
}

Tracker::BackgroundThread::BackgroundThread(
        std::shared_ptr<RecordWriter> record_writer,
        std::shared_ptr<RecordWriter> other_writer,
        unsigned int memory_interval,
        unsigned int cpu_interval)
: d_writer(std::move(record_writer))
, d_other_writer(std::move(other_writer))
, d_memory_interval(memory_interval)
, d_cpu_interval(cpu_interval)
{
#ifdef __linux__
    d_procs_statm.open("/proc/self/statm");
    if (!d_procs_statm) {
        throw IoError{"Failed to open /proc/self/statm"};
    }
#endif
}

unsigned long int
Tracker::BackgroundThread::timeElapsed()
{
    std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch());
    return ms.count();
}

size_t
Tracker::BackgroundThread::getRSS() const
{
#ifdef __linux__
    static long pagesize = sysconf(_SC_PAGE_SIZE);
    constexpr int max_unsigned_long_chars = std::numeric_limits<unsigned long>::digits10 + 1;
    constexpr int bufsize = (max_unsigned_long_chars + sizeof(' ')) * 2;
    char buffer[bufsize];
    d_procs_statm.read(buffer, sizeof(buffer) - 1);
    buffer[d_procs_statm.gcount()] = '\0';
    d_procs_statm.clear();
    d_procs_statm.seekg(0);

    size_t rss;
    if (sscanf(buffer, "%*u %zu", &rss) != 1) {
        std::cerr << "WARNING: Failed to read RSS value from /proc/self/statm" << std::endl;
        d_procs_statm.close();
        return 0;
    }

    return rss * pagesize;
#elif defined(__APPLE__)
    struct mach_task_basic_info info;
    mach_msg_type_number_t infoCount = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &infoCount)
        != KERN_SUCCESS)
        return (size_t)0L; /* Can't access? */
    return (size_t)info.resident_size;
#else
    return 0;
#endif
}

void
Tracker::BackgroundThread::start()
{
    assert(d_thread.get_id() == std::thread::id());
    d_thread = std::thread([&]() {
        MY_DEBUG("entering BackgroundThread::start  %llu >>>", std::this_thread::get_id());
        RecursionGuard::isActive = true;
        while (true) {
            {
                std::unique_lock<std::mutex> lock(d_mutex);
                d_cv.wait_for(lock, d_memory_interval * 1ms, [this]() { return d_stop; });
                if (d_stop) {
                    break;
                }
            }

            {
                size_t rss = getRSS();
                if (rss == 0) {
                    Tracker::deactivate();
                    break;
                }
                if (!d_writer->writeRecordMsg(MemoryRecord{timeElapsed(), rss})) {  // no use ?
                    std::cerr << "Failed to write output, deactivating tracking" << std::endl;
                    Tracker::deactivate();
                    break;
                }
            }
        }
        DebugInfo::printMemoryrecordDebugCnt();
    });
}

void
Tracker::BackgroundThread::startWriteRecord()
{
    assert(d_write_thread.get_id() == std::thread::id());
    d_write_thread = std::thread([&]() -> void {
        MY_DEBUG("entering BackgroundThread::startWriteRecord  %llu >>>", std::thread::id());
        RecursionGuard::isActive = true;
        Msg* msg_ptr = nullptr;
        while (true) {
            if (d_stop_writer) {
                break;
            };
            while (true) {
                msg_ptr = d_writer->getOneMsg(d_stop_writer);
                if (msg_ptr) {
                    break;
                }
                if (d_stop_writer) {
                    return;
                }
            }
            {
                RecursionGuard guard;
                bool ret = d_writer->procRecordMsg(msg_ptr);
                if (ret == false) {
                    continue;
                }
            }
            d_writer->popOneMsg();
        }
        DebugInfo::printProcessDebugCnt();
    });
}

void
Tracker::BackgroundThread::stop()
{
    {
        std::scoped_lock<std::mutex> lock(d_mutex);
        d_stop = true;
        d_cv.notify_one();
    }
    d_thread.join();
}

void
Tracker::BackgroundThread::stopWriteRecord()
{
    d_stop_writer = true;
    d_write_thread.join();
}

void
Tracker::prepareFork()
{
    // Don't do any custom track_allocation handling while inside fork
    RecursionGuard::isActive = true;
}

void
Tracker::parentFork()
{
    // We can continue tracking
    RecursionGuard::isActive = false;
}

void
Tracker::childFork()
{
    // Intentionally leak any old tracker. Its destructor cannot be called,
    // because it would try to destroy mutexes that might be locked by threads
    // that no longer exist, and to join a background thread that no longer
    // exists, and potentially to flush buffered output to a socket it no
    // longer owns. Note that d_instance_owner is always set after d_instance
    // and unset before d_instance.
    (void)d_instance_owner.release();

    Tracker* old_tracker = d_instance;

    // If we inherited an active tracker, try to clone its record writer.
    std::unique_ptr<RecordWriter> new_writer;
    std::unique_ptr<RecordWriter> new_native_writer;
    if (old_tracker && old_tracker->isActive() && old_tracker->d_follow_fork) {
        new_writer = old_tracker->d_writer->cloneInChildProcess();
        // new_native_writer = old_tracker->d_other_writer->cloneInChildProcess();
    }

    if (!new_writer) {
        // We either have no tracker, or a deactivated tracker, or a tracker
        // with a sink that can't be cloned. Unset our singleton and bail out.
        // Note that the old tracker's hooks may still be installed. This is
        // OK, as long as they always check the (static) isActive() flag before
        // calling any methods on the now null tracker singleton.
        Tracker::deactivate();
        d_instance = nullptr;
        RecursionGuard::isActive = false;
        return;
    }

    // Re-enable tracking with a brand new tracker.
    // Disable tracking until the new tracker is fully installed.
    Tracker::deactivate();
    d_instance_owner.reset(new Tracker(
            std::move(new_writer),
            std::move(new_native_writer),
            old_tracker->d_unwind_native_frames,
            old_tracker->d_memory_interval,
            old_tracker->d_follow_fork,
            old_tracker->d_trace_python_allocators));
    RecursionGuard::isActive = false;
}

void
Tracker::trackCpuImpl(hooks::Allocator func)  // func is just CPU_SAMPLING
{
    stopTrace();
    NativeTrace* cpu_trace_single = &NativeTrace::getInstance(1);
    if (!Tracker::isActive()
        || (cpu_trace_single->write_read_flag == NativeTrace::WRITE_READ_FLAG::READ_ONLY))
    {
        DebugInfo::blocked_cpu_sample++;
        return;
    }

    DebugInfo::processed_cpu_sample++;
    if (d_unwind_native_frames) {
        cpu_trace_single->fill(2);
        cpu_trace_single->backtrace_thread_id = d_writer->d_last.thread_id;
        cpu_trace_single->write_read_flag = NativeTrace::WRITE_READ_FLAG::READ_ONLY;
    } else {
        CpuSampleRecord record{func};
        if (!d_writer->writeThreadSpecificRecordMsg(thread_id(), record)) {
            std::cerr << "Failed to write output, deactivating tracking" << std::endl;
            deactivate();
        }
    }
    startTrace();
}

void
Tracker::trackAllocationImpl(void* ptr, size_t size, hooks::Allocator func)
{
    NativeTrace* mem_trace_single = &NativeTrace::getInstance(0);
    if (RecursionGuard::isActive || !Tracker::isActive()) {
        DebugInfo::blocked_allocation++;
        return;
    }
    RecursionGuard guard;
    //stopTrace();
    //PythonStackTracker::get().emitPendingPushesAndPops();
    DebugInfo::processed_allocation++;
    if (d_unwind_native_frames) {
        bool ret = mem_trace_single->fill(2);
        frame_id_t native_index = 0;
        mem_trace_single->backtrace_thread_id = d_writer->d_last.thread_id;
        if (ret) {
            native_index = d_writer->d_native_trace_tree.getTraceIndex(
                    mem_trace_single,
                    [&](frame_id_t ip, uint32_t index) {
                        return d_writer->writeUnresolvedNativeFrameMsg(UnresolvedNativeFrame{ip, index});
                    });
        }
        MY_DEBUG("mem - get frame tree native index: %lld", native_index);
        NativeAllocationRecord record{reinterpret_cast<uintptr_t>(ptr), size, func, native_index};
        if (!d_writer->writeThreadSpecificRecordMsg(thread_id(), record)) {
            std::cerr << "Failed to write output, deactivating tracking" << std::endl;
            deactivate();
        }
    } else {
        AllocationRecord record{reinterpret_cast<uintptr_t>(ptr), size, func};
        if (!d_writer->writeThreadSpecificRecordMsg(thread_id(), record)) {
            std::cerr << "Failed to write output, deactivating tracking" << std::endl;
            deactivate();
        }
    }
    //startTrace();
}

void
Tracker::trackDeallocationImpl(void* ptr, size_t size, hooks::Allocator func)
{
    if (RecursionGuard::isActive || !Tracker::isActive()) {
        return;
    }
    RecursionGuard guard;
    //stopTrace();
    AllocationRecord record{reinterpret_cast<uintptr_t>(ptr), size, func};
    if (!d_writer->writeThreadSpecificRecordMsg(thread_id(), record)) {
        std::cerr << "Failed to write output, deactivating tracking" << std::endl;
        deactivate();
    }
    //startTrace();
}

void
Tracker::invalidate_module_cache_impl()
{
    RecursionGuard guard;
    d_patcher.overwrite_symbols();
    updateModuleCache();
}

#ifdef __linux__
static int
dl_iterate_phdr_callback(struct dl_phdr_info* info, [[maybe_unused]] size_t size, void* data)
{
    auto writer = reinterpret_cast<RecordWriter*>(data);
    const char* filename = info->dlpi_name;  // object name
    std::string executable;
    assert(filename != nullptr);
    if (!filename[0]) {
        executable = get_executable();
        filename = executable.c_str();
    }
    if (::starts_with(filename, "linux-vdso.so")) {
        // This cannot be resolved to anything, so don't write it to the file
        return 0;
    }

    std::vector<Segment> segments;
    for (int i = 0; i < info->dlpi_phnum; i++) {  // head num
        const auto& phdr = info->dlpi_phdr[i];
        if (phdr.p_type == PT_LOAD) {
            segments.emplace_back(Segment{phdr.p_vaddr, phdr.p_memsz});
        }
    }

    if (!writer->writeRecordUnsafe(SegmentHeader{filename, segments.size(), info->dlpi_addr})) {
        std::cerr << "memray: Failed to write output, deactivating tracking" << std::endl;
        Tracker::deactivate();
        return 1;
    }

    /*
    if (!writer->writeRecordMsg(SegmentHeader{filename, segments.size(), info->dlpi_addr})) {
                std::cerr << "memray: Failed to write output, deactivating tracking" << std::endl;
                Tracker::deactivate();
                return 1;
    }
        */
    for (const auto& segment : segments) {
        if (!writer->writeRecordUnsafe(segment)) {
            std::cerr << "memray: Failed to write output, deactivating tracking" << std::endl;
            Tracker::deactivate();
            return 1;
        }
        /*
        if (!writer->writeRecordMsg(segment)) {
            std::cerr << "memray: Failed to write output, deactivating tracking" << std::endl;
            Tracker::deactivate();
            return 1;
        }
                */
    }

    return 0;
}
#endif

void
Tracker::updateModuleCacheImpl()
{
    if (!d_unwind_native_frames) {
        return;
    }
    auto writer_lock = d_writer->acquireLock();

    if (!d_writer->writeRecordUnsafe(MemoryMapStart{})) {
        std::cerr << "memray: Failed to write output, deactivating tracking" << std::endl;
        deactivate();
    }
    /*
    if (!d_writer->writeRecordMsg(MemoryMapStart{})) {
        std::cerr << "memray: Failed to write output, deactivating tracking" << std::endl;
        deactivate();
    }*/

    dl_iterate_phdr(
            &dl_iterate_phdr_callback,
            d_writer.get());  // https://www.onitroad.com/jc/linux/man-pages/linux/man3/dl_iterate_phdr.3.html
    // dl_iterate_phdr(&dl_iterate_phdr_callback, d_other_writer.get());
}

void
Tracker::registerThreadNameImpl(const char* name)
{
    /*
        if (!d_writer->writeThreadSpecificRecord(thread_id(), ThreadRecord{name})) {
        std::cerr << "memray: Failed to write output, deactivating tracking" << std::endl;
        deactivate();
    }
        */
    if (!d_writer->writeThreadSpecificRecordMsg(thread_id(), ThreadRecord{name})) {
        std::cerr << "memray: Failed to write output, deactivating tracking" << std::endl;
        deactivate();
    }
}

frame_id_t
Tracker::registerFrame(const RawFrame& frame)
{
    const auto [frame_id, is_new_frame] = d_frames.getIndex(frame);
    if (is_new_frame) {
        pyrawframe_map_val_t frame_index{frame_id, frame};
        /*
        if (!d_writer->writeRecord(frame_index)) {
            std::cerr << "memray: Failed to write output, deactivating tracking" << std::endl;
            deactivate();
        }
        */
        if (!d_writer->writeRecordMsg(frame_index)) {
            std::cerr << "memray: Failed to write output, deactivating tracking" << std::endl;
            deactivate();
        }
    }
    return frame_id;
}

bool
Tracker::popFrames(uint32_t count)
{
    /*
    const FramePop entry{count};
    if (!d_writer->writeThreadSpecificRecord(thread_id(), entry)) {
        std::cerr << "memray: Failed to write output, deactivating tracking" << std::endl;
        deactivate();
        return false;
    }
        return true;
    */
    while (count) {
        uint8_t to_pop = (count > 16 ? 16 : count);
        count -= to_pop;

        to_pop -= 1;  // i.e. 0 means pop 1 frame, 15 means pop 16 frames
        d_writer->writeRecordMsg(FramePop{to_pop});
    }
    return true;
}

bool
Tracker::pushFrame(const RawFrame& frame)
{
    const frame_id_t frame_id = registerFrame(frame);
    const FramePush entry{frame_id};
    /*
    if (!d_writer->writeThreadSpecificRecord(thread_id(), entry)) {
        std::cerr << "memray: Failed to write output, deactivating tracking" << std::endl;
        deactivate();
        return false;
    }
    */
    if (!d_writer->writeThreadSpecificRecordMsg(thread_id(), entry)) {
        std::cerr << "memray: Failed to write output, deactivating tracking" << std::endl;
        deactivate();
        return false;
    }
    return true;
}

void
Tracker::activate()
{
    d_active = true;
}

void
Tracker::deactivate()
{
    d_active = false;
}

const std::atomic<bool>&
Tracker::isActive()
{
    return Tracker::d_active;
}

// Static methods managing the singleton

PyObject*
Tracker::createTracker(
        std::unique_ptr<RecordWriter> record_writer,
        std::unique_ptr<RecordWriter> other_writer,
        bool native_traces,
        unsigned int memory_interval,
        bool follow_fork,
        bool trace_python_allocators)
{
    // Note: the GIL is used for synchronization of the singleton
    d_instance_owner.reset(new Tracker(
            std::move(record_writer),
            std::move(other_writer),
            native_traces,
            memory_interval,
            follow_fork,
            trace_python_allocators));
    PythonStackTracker::get().emitPendingPushesAndPops();
    MY_DEBUG("Tracker ins created && is activated");
    void* ptr = hooks::malloc(99999999);  // use SysMalloc
    void* ptr2 = malloc(8888888);  // use SysMalloc ? not sure
    void* ptr3 = intercept::malloc(6666666);
    MY_DEBUG("Tracker malloc type: %llu", &hooks::malloc);
    MY_DEBUG("Tracker malloc type: %llu", &malloc);
    MY_DEBUG("Tracker malloc type: %llu", intercept::malloc);
    if (ptr && ptr2 && ptr3) {
        MY_DEBUG("test native malloc succeed");
    } else {
        MY_DEBUG("test native malloc failed");
    }
    Py_RETURN_NONE;
}

PyObject*
Tracker::destroyTracker()
{
    // Note: the GIL is used for synchronization of the singleton
    d_instance_owner.reset();
    Py_RETURN_NONE;
}

Tracker*
Tracker::getTracker()
{
    return d_instance;
}

static struct
{
    PyMemAllocatorEx raw;
    PyMemAllocatorEx mem;
    PyMemAllocatorEx obj;
} s_orig_pymalloc_allocators;

void
Tracker::registerPymallocHooks() const noexcept
{
    assert(d_trace_python_allocators);
    PyMemAllocatorEx alloc;

    PyMem_GetAllocator(PYMEM_DOMAIN_RAW, &alloc);
    if (alloc.free == &intercept::pymalloc_free) {
        // Nothing to do; our hooks are already installed.
        return;
    }

    alloc.malloc = intercept::pymalloc_malloc;
    alloc.calloc = intercept::pymalloc_calloc;
    alloc.realloc = intercept::pymalloc_realloc;
    alloc.free = intercept::pymalloc_free;
    PyMem_GetAllocator(PYMEM_DOMAIN_RAW, &s_orig_pymalloc_allocators.raw);
    PyMem_GetAllocator(PYMEM_DOMAIN_MEM, &s_orig_pymalloc_allocators.mem);
    PyMem_GetAllocator(PYMEM_DOMAIN_OBJ, &s_orig_pymalloc_allocators.obj);
    alloc.ctx = &s_orig_pymalloc_allocators.raw;
    PyMem_SetAllocator(PYMEM_DOMAIN_RAW, &alloc);
    alloc.ctx = &s_orig_pymalloc_allocators.mem;
    PyMem_SetAllocator(PYMEM_DOMAIN_MEM, &alloc);
    alloc.ctx = &s_orig_pymalloc_allocators.obj;
    PyMem_SetAllocator(PYMEM_DOMAIN_OBJ, &alloc);
}

void
Tracker::unregisterPymallocHooks() const noexcept
{
    assert(d_trace_python_allocators);
    PyMem_SetAllocator(PYMEM_DOMAIN_RAW, &s_orig_pymalloc_allocators.raw);
    PyMem_SetAllocator(PYMEM_DOMAIN_MEM, &s_orig_pymalloc_allocators.mem);
    PyMem_SetAllocator(PYMEM_DOMAIN_OBJ, &s_orig_pymalloc_allocators.obj);
}

// Trace Function interface

PyObject*
create_profile_arg()
{
    // Borrowed reference
    PyObject* memray_ext = PyDict_GetItemString(PyImport_GetModuleDict(), "memray._memray");
    if (!memray_ext) {
        return nullptr;
    }

    return PyObject_CallMethod(memray_ext, "ProfileFunctionGuard", nullptr);
}

// Called when profiling is initially enabled in each thread.
int
PyTraceTrampoline(PyObject* obj, PyFrameObject* frame, int what, [[maybe_unused]] PyObject* arg)
{
    assert(PyGILState_Check());
    RecursionGuard guard;

    PyObject* profileobj = create_profile_arg();
    if (!profileobj) {
        return -1;
    }
    PyEval_SetProfile(PyTraceFunction, profileobj);
    Py_DECREF(profileobj);

    return PyTraceFunction(obj, frame, what, profileobj);
}

int
PyTraceFunction(
        [[maybe_unused]] PyObject* obj,
        PyFrameObject* frame,
        int what,
        [[maybe_unused]] PyObject* arg)
{
    RecursionGuard guard;
    if (!Tracker::isActive()) {
        return 0;
    }

    if (frame != PyEval_GetFrame()) {
        return 0;
    }

    switch (what) {
        case PyTrace_CALL: {
            return PythonStackTracker::get().pushPythonFrame(frame);
        }
        case PyTrace_RETURN: {
            PythonStackTracker::get().popPythonFrame();
            break;
        }
        default:
            break;
    }
    return 0;
}

void
forget_python_stack()
{
    if (!Tracker::isActive()) {
        return;
    }

    RecursionGuard guard;
    PythonStackTracker::get().clear();
}

void
begin_tracking_greenlets()
{
    assert(PyGILState_Check());
    PythonStackTracker::s_greenlet_tracking_enabled = true;
}

void
handle_greenlet_switch(PyObject* from, PyObject* to)
{
    PythonStackTracker::get().handleGreenletSwitch(from, to);
}

void
install_trace_function()
{
    assert(PyGILState_Check());
    RecursionGuard guard;
    // Don't clear the python stack if we have already registered the tracking
    // function with the current thread. This happens when PyGILState_Ensure is
    // called and a thread state with our hooks installed already exists.
    PyThreadState* ts = PyThreadState_Get();
    if (ts->c_profilefunc == PyTraceFunction) {
        return;
    }
    PyObject* profileobj = create_profile_arg();
    if (!profileobj) {
        return;
    }
    PyEval_SetProfile(PyTraceFunction, profileobj);
    Py_DECREF(profileobj);

    PyFrameObject* frame = PyEval_GetFrame();

    // Push all of our Python frames, most recent last.  If we reached here
    // from PyGILState_Ensure on a C thread there may be no Python frames.
    std::vector<PyFrameObject*> stack;
    while (frame) {
        stack.push_back(frame);
        frame = compat::frameGetBack(frame);
    }
    auto& python_stack_tracker = PythonStackTracker::get();
    for (auto frame_it = stack.rbegin(); frame_it != stack.rend(); ++frame_it) {
        python_stack_tracker.pushPythonFrame(*frame_it);
    }

    python_stack_tracker.installGreenletTraceFunctionIfNeeded();
}

}  // namespace memray::tracking_api
