#pragma once

#include <cerrno>
#include <chrono>
#include <climits>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <type_traits>
#include <unistd.h>

#include "frame_tree.h"
#include "records.h"
#include "sink.h"

namespace memray::tracking_api {

struct SegmentHeaderInfo
{
    // std::vector<char> filename = decltype(filename)(256);
    char filename[256];
    size_t num_segments;
    uintptr_t addr;
};

struct Msg
{
    RecordTypeAndFlags token;
    bool seek_to_start;
    HeaderRecord d_header{};
    DeltaEncodedFields d_last;
    MemoryRecord d_memory_record{};
    UnresolvedNativeFrame d_native_frame_record;
    FramePush d_frame_push;
    RawFrameInfo d_pyrawframe;
    FramePop d_frame_pop;
    AllocationRecord d_allocation;
    NativeAllocationRecord d_native_allocation;
    frame_id_t native_frame_id{0};
    CpuSampleRecord d_cpu_sample;
    NativeCpuSampleRecord d_native_cpu_sample;
    ContextSwitch d_cs;
    // std::vector<char> d_thread_name = decltype(d_thread_name)(64);
    char d_thread_name[64];
    SegmentHeaderInfo d_sgh;
    Segment d_sg;
};
using MsgQ = SPSCQueueOPT<Msg, 4096>;

class RecordWriter
{
  public:
    explicit RecordWriter(
            std::unique_ptr<memray::io::Sink> sink,
            const std::string& command_line,
            bool native_traces);

    RecordWriter(RecordWriter& other) = delete;
    RecordWriter(RecordWriter&& other) = delete;
    void operator=(const RecordWriter&) = delete;
    void operator=(RecordWriter&&) = delete;

    Msg* getOneAvaiableMsgNode()
    {
        while (true) {
            Msg* node = d_msg_q->alloc();
            if (node) {
                DebugInfo::total_used_msg_node++;
                return node;
            }
            DebugInfo::get_avaiable_msg_node_failed++;
        }
        return nullptr;
    }

    void pushMsgNode()
    {
        d_msg_q->push();
    }

    Msg* getOneMsg(bool stop)
    {
        Msg* node = d_msg_q->front();
        return node;
    }

    bool inline procRecordMsg(const Msg* msg);

    void inline popOneMsg()
    {
        d_msg_q->pop();
    }

    template<typename T>
    bool inline writeSimpleType(const T& item);
    bool inline writeString(const char* the_string);
    bool inline writeVarint(size_t val);
    bool inline writeSignedVarint(ssize_t val);
    template<typename T>
    bool inline writeIntegralDelta(T* prev, T new_val);
    template<typename T>
    bool inline writeIntegralDelta2(const T& prev, T new_val);

    template<typename T>
    bool inline writeRecord(const T& item);
    template<typename T>
    bool inline writeRecordMsg(const T& item);

    template<typename T>
    bool inline writeMsgWithContext(thread_id_t& last_tid, thread_id_t tid, const T& item);
    bool inline writeUnresolvedNativeFrameMsg(const UnresolvedNativeFrame& item);

    template<typename T>
    bool inline writeThreadSpecificRecord(thread_id_t tid, const T& item);
    template<typename T>
    bool inline writeThreadSpecificRecordMsg(thread_id_t tid, const T& item);

    bool inline writeRecordUnsafe(const FramePop& record);
    bool inline writeRecordMsgUnsafe(Msg* msg, const FramePop& record);
    bool inline procFramePopMsg(const RecordTypeAndFlags& token, const FramePop& record);

    bool inline writeRecordUnsafe(const FramePush& record);
    bool inline writeRecordMsgUnsafe(Msg* msg, const FramePush& record);
    bool inline procFramePushMsg(
            const RecordTypeAndFlags& token,
            const DeltaEncodedFields& last,
            const FramePush& record);

    bool inline writeRecordUnsafe(const MemoryRecord& record);
    bool inline writeRecordMsgUnsafe(Msg* msg, const MemoryRecord& record);
    bool inline procMemoryRecordMsg(
            const RecordTypeAndFlags& token,
            const TrackerStats& stats,
            const MemoryRecord& record);

    bool inline writeRecordUnsafe(const CpuRecord& record);
    bool inline writeRecordMsgUnsafe(Msg* msg, const CpuRecord& record);
    bool inline procCpuRecordMsg();

    bool inline writeRecordUnsafe(const CpuSampleRecord& record);
    bool inline writeRecordMsgUnsafe(Msg* msg, const CpuSampleRecord& record);
    bool inline procCpuSampleMsg(const RecordTypeAndFlags& token);

    bool inline writeRecordUnsafe(const NativeCpuSampleRecord& record);
    bool inline writeRecordMsgUnsafe(Msg* msg, const NativeCpuSampleRecord& record);
    bool inline procNativeCpuSampleMsg(
            const RecordTypeAndFlags& token,
            const DeltaEncodedFields& last,
            const NativeCpuSampleRecord& record);

    bool inline writeRecordUnsafe(const ContextSwitch& record);
    bool inline writeRecordMsgUnsafe(Msg* msg, const ContextSwitch& record);
    bool inline procContextSwitchMsg(const RecordTypeAndFlags& token, const ContextSwitch& record);

    bool inline writeRecordUnsafe(const Segment& record);
    bool inline writeRecordMsgUnsafe(Msg* msg, const Segment& record);
    bool inline procSegmentMsg(const RecordTypeAndFlags& token, const Segment& record);

    bool inline writeRecordUnsafe(const AllocationRecord& record);
    bool inline writeRecordMsgUnsafe(Msg* msg, const AllocationRecord& record);
    bool inline procAllocationMsg(
            const RecordTypeAndFlags& token,
            const DeltaEncodedFields& last,
            const AllocationRecord& record);

    bool inline writeRecordUnsafe(const NativeAllocationRecord& record);
    bool inline writeRecordMsgUnsafe(Msg* msg, const NativeAllocationRecord& record);
    bool inline procNativeAllocationMsg(
            const RecordTypeAndFlags& token,
            const DeltaEncodedFields& last,
            const NativeAllocationRecord& record);

    bool inline writeRecordUnsafe(const pyrawframe_map_val_t& item);
    bool inline writeRecordMsgUnsafe(Msg* msg, const pyrawframe_map_val_t& item);  // for memory
    bool inline procFrameIndexMsg(
            const RecordTypeAndFlags& token,
            const DeltaEncodedFields& last,
            const RawFrameInfo& record);

    bool inline writeRecordUnsafe(const SegmentHeader& item);
    bool inline writeRecordMsgUnsafe(Msg* msg, const SegmentHeader& item);
    bool inline procSegmentHeaderMsg(const RecordTypeAndFlags& token, const SegmentHeaderInfo& item);

    bool inline writeRecordUnsafe(const ThreadRecord& record);
    bool inline writeRecordMsgUnsafe(Msg* msg, const ThreadRecord& record);
    bool inline procThreadRecordMsg(const RecordTypeAndFlags& token, const char* thread_name);

    bool inline writeRecordUnsafe(const UnresolvedNativeFrame& record);
    bool inline writeRecordMsgUnsafe(Msg* msg, const UnresolvedNativeFrame& record);
    bool inline procUnresolvedNativeFrameMsg(
            const RecordTypeAndFlags& token,
            const DeltaEncodedFields& last,
            const UnresolvedNativeFrame& record);

    bool inline writeRecordUnsafe(const MemoryMapStart&);
    bool inline writeRecordMsgUnsafe(Msg* msg, const MemoryMapStart&);
    bool inline procMemoryMapStartMsg(const RecordTypeAndFlags& token);

    bool writeHeader(bool seek_to_start);
    bool writeHeaderMsg(bool seek_to_start);
    bool procHeaderMsg(bool seek_to_start, const HeaderRecord& header);

    bool writeTrailer();
    bool inline writeTrailerMsg();
    bool inline procTrailerMsg(const RecordTypeAndFlags& token);

    std::unique_lock<std::mutex> acquireLock();
    std::unique_ptr<RecordWriter> cloneInChildProcess();

  public:
    // Data members
    int d_version{CURRENT_HEADER_VERSION};
    std::unique_ptr<memray::io::Sink> d_sink;
    std::shared_ptr<MsgQ> d_msg_q;
    std::mutex d_mutex;
    SpinMutex d_spin_mutex;
    HeaderRecord d_header{};
    TrackerStats d_stats{};
    DeltaEncodedFields d_last;

  public:
    size_t pop_frames_cnt{0};
    std::vector<RawFrame> raw_frames;
    size_t raw_frames_cnt{0};

    FrameCollection<RawFrame> d_frames;
    FrameTree d_native_trace_tree;
};

template<typename T>
bool inline RecordWriter::writeSimpleType(const T& item)
{
    static_assert(
            std::is_trivially_copyable<T>::value,
            "writeSimpleType called on non trivially copyable type");

    return d_sink->writeAll(reinterpret_cast<const char*>(&item), sizeof(item));
};

bool inline RecordWriter::writeString(const char* the_string)
{
    return d_sink->writeAll(the_string, strlen(the_string) + 1);
}

bool inline RecordWriter::writeVarint(size_t rest)
{
    unsigned char next_7_bits = rest & 0x7f;
    rest >>= 7;
    while (rest) {
        next_7_bits |= 0x80;
        if (!writeSimpleType(next_7_bits)) {
            return false;
        }
        next_7_bits = rest & 0x7f;
        rest >>= 7;
    }

    return writeSimpleType(next_7_bits);
}

bool inline RecordWriter::writeSignedVarint(ssize_t val)
{
    // protobuf style "zig-zag" encoding
    // https://developers.google.com/protocol-buffers/docs/encoding#signed-ints
    // This encodes -64 through 63 in 1 byte, -8192 through 8191 in 2 bytes, etc
    size_t zigzag_val = (static_cast<size_t>(val) << 1)
                        ^ static_cast<size_t>(val >> std::numeric_limits<ssize_t>::digits);
    return writeVarint(zigzag_val);
}

template<typename T>
bool inline RecordWriter::writeIntegralDelta(T* prev, T new_val)
{
    ssize_t delta = new_val - *prev;
    *prev = new_val;
    return writeSignedVarint(delta);
}

template<typename T>
bool inline RecordWriter::writeIntegralDelta2(const T& prev, T new_val)
{
    ssize_t delta = new_val - prev;
    if (delta < 0) {
        assert("1 == 2");
    }
    return writeSignedVarint(delta);
}

bool inline RecordWriter::procRecordMsg(const Msg* msg)
{
    if (msg == nullptr) {
        return false;
    }
    DebugInfo::total_processed_msg++;

    switch (msg->token.record_type) {
        case RecordType::OTHER: {
            switch (static_cast<OtherRecordType>(msg->token.flags)) {
                case OtherRecordType::HEADER: {
                    procHeaderMsg(msg->seek_to_start, msg->d_header);
                } break;
                case OtherRecordType::TRAILER: {
                    procTrailerMsg(msg->token);
                } break;
            }
        } break;
        case RecordType::MEMORY_MAP_START: {
            procMemoryMapStartMsg(msg->token);
        } break;
        case RecordType::SEGMENT_HEADER: {
            procSegmentHeaderMsg(msg->token, msg->d_sgh);
        } break;
        case RecordType::SEGMENT: {
            procSegmentMsg(msg->token, msg->d_sg);
        } break;
        case RecordType::FRAME_PUSH: {
            procFramePushMsg(msg->token, msg->d_last, msg->d_frame_push);
        } break;
        case RecordType::FRAME_INDEX: {
            procFrameIndexMsg(msg->token, msg->d_last, msg->d_pyrawframe);
        } break;
        case RecordType::FRAME_POP: {
            procFramePopMsg(msg->token, msg->d_frame_pop);
        } break;
        case RecordType::ALLOCATION: {
            procAllocationMsg(msg->token, msg->d_last, msg->d_allocation);
        } break;
        case RecordType::ALLOCATION_WITH_NATIVE: {
            procNativeAllocationMsg(msg->token, msg->d_last, msg->d_native_allocation);
        } break;
        case RecordType::CPU_SAMPLE: {
            procCpuSampleMsg(msg->token);
        } break;
        case RecordType::CPU_SAMPLE_WITH_NATIVE: {
            procNativeCpuSampleMsg(msg->token, msg->d_last, msg->d_native_cpu_sample);
        } break;
        case RecordType::MEMORY_RECORD: {
            procMemoryRecordMsg(msg->token, msg->d_header.stats, msg->d_memory_record);
        } break;
        case RecordType::CPU_RECORD: {
            procCpuRecordMsg();
        } break;
        case RecordType::NATIVE_TRACE_INDEX: {
            procUnresolvedNativeFrameMsg(msg->token, msg->d_last, msg->d_native_frame_record);
        } break;
        case RecordType::CONTEXT_SWITCH: {
            procContextSwitchMsg(msg->token, msg->d_cs);
        } break;
        case RecordType::THREAD_RECORD: {
            procThreadRecordMsg(msg->token, msg->d_thread_name);
        } break;
        default:
            break;
    }
    return true;
}

bool inline RecordWriter::writeHeaderMsg(bool seek_to_start)
{
    Msg* msg = getOneAvaiableMsgNode();
    msg->seek_to_start = seek_to_start;
    msg->token = {RecordType::OTHER, int(OtherRecordType::HEADER)};
    d_stats.end_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::system_clock::now().time_since_epoch())
                               .count();
    d_stats.cpu_profiler_end_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                                            std::chrono::system_clock::now().time_since_epoch())
                                            .count();
    msg->d_header = d_header;  // stuct deep copy
    msg->d_header.stats = d_stats;
    pushMsgNode();
    return true;
}

bool inline RecordWriter::procHeaderMsg(bool seek_to_start, const HeaderRecord& header)
{
    if (seek_to_start) {
        if (!d_sink->seek(0, SEEK_SET)) {
            return false;
        }
    }
    if (!writeSimpleType(header.magic) or !writeSimpleType(header.version)
        or !writeSimpleType(header.native_traces) or !writeSimpleType(header.stats)
        or !writeString(header.command_line.c_str()) or !writeSimpleType(header.pid)
        or !writeSimpleType(header.python_allocator))
    {
        return false;
    }
    return true;
}

bool inline RecordWriter::writeTrailerMsg()
{
    Msg* msg = getOneAvaiableMsgNode();
    msg->token = {RecordType::OTHER, int(OtherRecordType::TRAILER)};
    pushMsgNode();
    return true;
}

bool inline RecordWriter::procTrailerMsg(const RecordTypeAndFlags& token)
{
    return writeSimpleType(token);
}

template<typename T>
bool inline RecordWriter::writeRecord(const T& item)
{
    std::lock_guard<std::mutex> lock(d_mutex);
    bool ret = writeRecordUnsafe(item);
    return ret;
}

template<typename T>
bool inline RecordWriter::writeMsgWithContext(thread_id_t& last_tid, thread_id_t tid, const T& item)
{
    std::lock_guard<std::mutex> lock(d_mutex);
    if (last_tid != tid) {
        last_tid = tid;
        Msg* msg = getOneAvaiableMsgNode();
        writeRecordMsgUnsafe(msg, ContextSwitch{tid});
        pushMsgNode();
    }
    Msg* msg = getOneAvaiableMsgNode();
    bool ret = writeRecordMsgUnsafe(msg, item);
    pushMsgNode();
    return ret;
}

bool inline RecordWriter::writeUnresolvedNativeFrameMsg(const UnresolvedNativeFrame& item)
{
    std::lock_guard<std::mutex> lock(d_mutex);
    Msg* msg = getOneAvaiableMsgNode();
    bool ret = writeRecordMsgUnsafe(msg, item);
    pushMsgNode();
    return ret;
}

template<typename T>
bool inline RecordWriter::writeRecordMsg(const T& item)  // multi threads write
{
    std::lock_guard<std::mutex> lock(d_mutex);
    static Timer t;
    t.now();
    Msg* msg = getOneAvaiableMsgNode();
    bool ret = writeRecordMsgUnsafe(msg, item);
    pushMsgNode();
    DebugInfo::write_record_msg_time += t.elapsedNs();
    return ret;
}

template<typename T>
bool inline RecordWriter::writeThreadSpecificRecordMsg(
        thread_id_t tid,
        const T& item)  // only one thread write
{
    Timer t;
    t.now();
    NativeTrace* cpu_trace_single = &NativeTrace::getInstance(1);
    if (cpu_trace_single->write_read_flag == NativeTrace::WRITE_READ_FLAG::READ_ONLY) {
        while (pop_frames_cnt) {
            uint8_t to_pop = (pop_frames_cnt > 16 ? 16 : pop_frames_cnt);
            pop_frames_cnt -= to_pop;
            to_pop -= 1;  // i.e. 0 means pop 1 frame, 15 means pop 16 frames
            writeMsgWithContext(
                    d_last.thread_id,
                    cpu_trace_single->backtrace_thread_id,
                    FramePop{to_pop});
        }

        for (int32_t i = 0; i < raw_frames_cnt; i++) {
            const auto [frame_id, is_new_frame] = d_frames.getIndex(raw_frames[i]);
            if (is_new_frame) {
                const pyrawframe_map_val_t& frame_index{frame_id, raw_frames[i]};
                writeRecordMsg(frame_index);
            }
            writeMsgWithContext(
                    d_last.thread_id,
                    cpu_trace_single->backtrace_thread_id,
                    FramePush{frame_id});
        }
        raw_frames_cnt = 0;
        pop_frames_cnt = 0;

        frame_id_t native_index = 0;
        native_index =
                d_native_trace_tree.getTraceIndex(cpu_trace_single, [&](frame_id_t ip, uint32_t index) {
                    return writeUnresolvedNativeFrameMsg(UnresolvedNativeFrame{ip, index});
                });
        // MY_DEBUG("cpu - get frame tree native index: %lld", native_index);
        NativeCpuSampleRecord record{hooks::Allocator::CPU_SAMPLING, native_index};
        bool ret = writeMsgWithContext(d_last.thread_id, cpu_trace_single->backtrace_thread_id, record);
        cpu_trace_single->write_read_flag = NativeTrace::WRITE_READ_FLAG::WRITE_ONLY;
    }
    bool ret = writeMsgWithContext(d_last.thread_id, tid, item);
    DebugInfo::write_threadspecific_record_msg_time += t.elapsedNs();
    return ret;
}

template<typename T>
bool inline RecordWriter::writeThreadSpecificRecord(thread_id_t tid, const T& item)
{
    std::lock_guard<std::mutex> lock(d_mutex);
    if (d_last.thread_id != tid) {
        d_last.thread_id = tid;
        if (!writeRecordUnsafe(ContextSwitch{tid})) {
            return false;
        }
    }
    bool ret = writeRecordUnsafe(item);
    return ret;
}

bool inline RecordWriter::writeRecordUnsafe(const FramePop& record)
{
    size_t count = record.count;
    while (count) {
        uint8_t to_pop = (count > 16 ? 16 : count);
        count -= to_pop;

        to_pop -= 1;  // i.e. 0 means pop 1 frame, 15 means pop 16 frames
        RecordTypeAndFlags token{RecordType::FRAME_POP, to_pop};
        assert(token.flags == to_pop);
        if (!writeSimpleType(token)) {
            return false;
        }
    }

    return true;
}

bool inline RecordWriter::writeRecordMsgUnsafe(Msg* msg, const FramePop& record)
{
    DebugInfo::write_frame_pop_msg++;

    msg->token = {RecordType::FRAME_POP, record.count};
    return true;
}

bool inline RecordWriter::procFramePopMsg(const RecordTypeAndFlags& token, const FramePop& record)
{
    bool ret = writeSimpleType(token);
    return ret;
}

bool inline RecordWriter::writeRecordUnsafe(const FramePush& record)
{
    RecordTypeAndFlags token{RecordType::FRAME_PUSH, 0};
    return writeSimpleType(token) && writeIntegralDelta2(d_last.python_frame_id, record.frame_id);
}

bool inline RecordWriter::writeRecordMsgUnsafe(Msg* msg, const FramePush& record)
{
    DebugInfo::write_frame_push_msg++;

    msg->token = {RecordType::FRAME_PUSH, 0};
    msg->d_last.python_frame_id = d_last.python_frame_id;
    msg->d_frame_push.frame_id = record.frame_id;

    d_last.python_frame_id = record.frame_id;
    return true;
}

bool inline RecordWriter::procFramePushMsg(
        const RecordTypeAndFlags& token,
        const DeltaEncodedFields& last,
        const FramePush& record)
{
    bool ret = writeSimpleType(token) && writeIntegralDelta2(last.python_frame_id, record.frame_id);
    return ret;
}

bool inline RecordWriter::writeRecordUnsafe(const MemoryRecord& record)
{
    RecordTypeAndFlags token{RecordType::MEMORY_RECORD, 0};
    return writeSimpleType(token) && writeVarint(record.rss)
           && writeVarint(record.ms_since_epoch - d_stats.start_time) && d_sink->flush();
}

bool inline RecordWriter::writeRecordMsgUnsafe(Msg* msg, const MemoryRecord& record)
{
    DebugInfo::write_memory_record_msg++;

    msg->token = {RecordType::MEMORY_RECORD, 0};
    msg->d_memory_record.ms_since_epoch = record.ms_since_epoch;
    msg->d_memory_record.rss = record.rss;
    return true;
}

bool inline RecordWriter::procMemoryRecordMsg(
        const RecordTypeAndFlags& token,
        const TrackerStats& stats,
        const MemoryRecord& record)
{
    bool ret = writeSimpleType(token) && writeVarint(record.rss)
               && writeVarint(record.ms_since_epoch - stats.start_time) && d_sink->flush();
    return ret;
}

bool inline RecordWriter::writeRecordMsgUnsafe(Msg* msg, const CpuRecord& record)
{
    return true;
}

bool inline RecordWriter::procCpuRecordMsg()
{
    return true;
}

bool inline RecordWriter::writeRecordUnsafe(const CpuRecord& record)
{
    RecordTypeAndFlags token{RecordType::CPU_RECORD, 0};
    return writeSimpleType(token) && writeVarint(record.ms_since_epoch - d_stats.cpu_profiler_start_time)
           && d_sink->flush();
}

bool inline RecordWriter::writeRecordUnsafe(const ContextSwitch& record)
{
    RecordTypeAndFlags token{RecordType::CONTEXT_SWITCH, 0};
    return writeSimpleType(token) && writeSimpleType(record);
}

bool inline RecordWriter::writeRecordMsgUnsafe(Msg* msg, const ContextSwitch& record)
{
    msg->token = {RecordType::CONTEXT_SWITCH, 0};
    msg->d_cs = record;
    return true;
}

bool inline RecordWriter::procContextSwitchMsg(
        const RecordTypeAndFlags& token,
        const ContextSwitch& record)
{
    bool ret = writeSimpleType(token) && writeSimpleType(record);
    return ret;
}

bool inline RecordWriter::writeRecordUnsafe(const Segment& record)
{
    DebugInfo::write_segment++;

    RecordTypeAndFlags token{RecordType::SEGMENT, 0};
    return writeSimpleType(token) && writeSimpleType(record.vaddr) && writeVarint(record.memsz);
}

bool inline RecordWriter::writeRecordMsgUnsafe(Msg* msg, const Segment& record)
{
    msg->token = {RecordType::SEGMENT, 0};

    msg->d_sg.vaddr = record.vaddr;
    msg->d_sg.memsz = record.memsz;
    return true;
}

bool inline RecordWriter::procSegmentMsg(const RecordTypeAndFlags& token, const Segment& record)
{
    bool ret = writeSimpleType(token) && writeSimpleType(record.vaddr) && writeVarint(record.memsz);
    return ret;
}

bool inline RecordWriter::writeRecordUnsafe(const CpuSampleRecord& record)
{
    d_stats.n_cpu_samples += 1;
    RecordTypeAndFlags token{RecordType::CPU_SAMPLE, static_cast<unsigned char>(record.allocator)};
    return writeSimpleType(token);
}

bool inline RecordWriter::writeRecordMsgUnsafe(Msg* msg, const CpuSampleRecord& record)
{
    d_stats.n_cpu_samples += 1;

    msg->token = {RecordType::CPU_SAMPLE, static_cast<unsigned char>(record.allocator)};
    return true;
}

bool inline RecordWriter::procCpuSampleMsg(const RecordTypeAndFlags& token)
{
    bool ret = writeSimpleType(token);
    return ret;
}

bool inline RecordWriter::writeRecordUnsafe(const NativeCpuSampleRecord& record)
{
    d_stats.n_cpu_samples += 1;
    RecordTypeAndFlags token{
            RecordType::CPU_SAMPLE_WITH_NATIVE,
            static_cast<unsigned char>(record.allocator)};
    return writeSimpleType(token) && writeIntegralDelta(&d_last.native_frame_id, record.native_frame_id);
}

bool inline RecordWriter::writeRecordMsgUnsafe(Msg* msg, const NativeCpuSampleRecord& record)
{
    d_stats.n_cpu_samples += 1;
    msg->token = {RecordType::CPU_SAMPLE_WITH_NATIVE, static_cast<unsigned char>(record.allocator)};
    msg->d_last.native_frame_id = d_last.native_frame_id;
    msg->d_native_cpu_sample.native_frame_id = record.native_frame_id;

    d_last.native_frame_id = record.native_frame_id;
    return true;
}

bool inline RecordWriter::procNativeCpuSampleMsg(
        const RecordTypeAndFlags& token,
        const DeltaEncodedFields& last,
        const NativeCpuSampleRecord& record)
{
    bool ret =
            writeSimpleType(token) && writeIntegralDelta2(last.native_frame_id, record.native_frame_id);
    return ret;
}

bool inline RecordWriter::writeRecordUnsafe(const AllocationRecord& record)
{
    d_stats.n_allocations += 1;
    RecordTypeAndFlags token{RecordType::ALLOCATION, static_cast<unsigned char>(record.allocator)};
    return writeSimpleType(token) && writeIntegralDelta(&d_last.data_pointer, record.address)
           && (hooks::allocatorKind(record.allocator) == hooks::AllocatorKind::SIMPLE_DEALLOCATOR
               || writeVarint(record.size));
}

bool inline RecordWriter::writeRecordMsgUnsafe(Msg* msg, const AllocationRecord& record)
{
    DebugInfo::write_allocation_msg++;

    d_stats.n_allocations += 1;

    msg->token = {RecordType::ALLOCATION, static_cast<unsigned char>(record.allocator)};
    msg->d_allocation.allocator = record.allocator;
    msg->d_allocation.address = record.address;
    msg->d_allocation.size = record.size;
    msg->d_last.data_pointer = d_last.data_pointer;

    d_last.data_pointer = record.address;
    return true;
}

bool inline RecordWriter::procAllocationMsg(
        const RecordTypeAndFlags& token,
        const DeltaEncodedFields& last,
        const AllocationRecord& record)
{
    bool ret = writeSimpleType(token) && writeIntegralDelta2(last.data_pointer, record.address)
               && (hooks::allocatorKind(record.allocator) == hooks::AllocatorKind::SIMPLE_DEALLOCATOR
                   || writeVarint(record.size));
    return ret;
}

bool inline RecordWriter::writeRecordUnsafe(const NativeAllocationRecord& record)
{
    d_stats.n_allocations += 1;
    RecordTypeAndFlags token{
            RecordType::ALLOCATION_WITH_NATIVE,
            static_cast<unsigned char>(record.allocator)};
    return writeSimpleType(token) && writeIntegralDelta(&d_last.data_pointer, record.address)
           && writeVarint(record.size)
           && writeIntegralDelta(&d_last.native_frame_id, record.native_frame_id);
}

bool inline RecordWriter::writeRecordMsgUnsafe(Msg* msg, const NativeAllocationRecord& record)
{
    DebugInfo::write_native_allocation_msg++;

    d_stats.n_allocations += 1;
    msg->token = {RecordType::ALLOCATION_WITH_NATIVE, static_cast<unsigned char>(record.allocator)};

    msg->d_last.native_frame_id = d_last.native_frame_id;
    msg->d_last.data_pointer = d_last.data_pointer;

    msg->d_native_allocation.address = record.address;
    msg->d_native_allocation.size = record.size;
    msg->d_native_allocation.native_frame_id = record.native_frame_id;

    d_last.native_frame_id = record.native_frame_id;
    d_last.data_pointer = record.address;
    return true;
}

bool inline RecordWriter::procNativeAllocationMsg(
        const RecordTypeAndFlags& token,
        const DeltaEncodedFields& last,
        const NativeAllocationRecord& record)
{
    bool ret = writeSimpleType(token) && writeIntegralDelta2(last.data_pointer, record.address)
               && writeVarint(record.size)
               && writeIntegralDelta2(last.native_frame_id, record.native_frame_id);
    return ret;
}

bool inline RecordWriter::writeRecordUnsafe(const pyrawframe_map_val_t& item)
{
    d_stats.n_frames += 1;
    RecordTypeAndFlags token{RecordType::FRAME_INDEX, !item.second.is_entry_frame};
    return writeSimpleType(token) && writeIntegralDelta(&d_last.python_frame_id, item.first)
           && writeString(item.second.function_name) && writeString(item.second.filename)
           && writeIntegralDelta(&d_last.python_line_number, item.second.lineno);
}

bool inline RecordWriter::writeRecordMsgUnsafe(Msg* msg,
                                               const pyrawframe_map_val_t& item)  // RawFrame
{
    DebugInfo::write_pyrawframe_msg++;

    d_stats.n_frames += 1;

    msg->token = {RecordType::FRAME_INDEX, !item.second.is_entry_frame};
    msg->d_last.python_frame_id = d_last.python_frame_id;
    msg->d_last.python_line_number = d_last.python_line_number;

    d_last.python_frame_id = item.first;
    d_last.python_line_number = item.second.lineno;

    msg->d_pyrawframe.frame_id = item.first;
    msg->d_pyrawframe.lineno = item.second.lineno;

    // size_t n = strlen(item.second.filename) + 1;
    // msg->d_pyrawframe.filename = (char *)malloc(sizeof(char) * n);
    // memcpy(msg->d_pyrawframe.filename, item.second.filename, n);
    copyChar(msg->d_pyrawframe.filename, item.second.filename);  // notice length overflow

    // n = strlen(item.second.function_name) + 1;
    // msg->d_pyrawframe.function_name = (char *)malloc(sizeof(char) * n);
    // memcpy(msg->d_pyrawframe.function_name, item.second.function_name, n);
    copyChar(msg->d_pyrawframe.function_name, item.second.function_name);

    return true;
}

bool inline RecordWriter::procFrameIndexMsg(
        const RecordTypeAndFlags& token,
        const DeltaEncodedFields& last,
        const RawFrameInfo& record)
{
    bool ret = writeSimpleType(token) && writeIntegralDelta2(last.python_frame_id, record.frame_id)
               && writeString(record.function_name) && writeString(record.filename)
               && writeIntegralDelta2(last.python_line_number, record.lineno);
    return ret;
}

bool inline RecordWriter::writeRecordUnsafe(const SegmentHeader& item)
{
    DebugInfo::write_segment_header++;

    RecordTypeAndFlags token{RecordType::SEGMENT_HEADER, 0};
    return writeSimpleType(token) && writeString(item.filename) && writeVarint(item.num_segments)
           && writeSimpleType(item.addr);
}

bool inline RecordWriter::writeRecordMsgUnsafe(Msg* msg, const SegmentHeader& record)
{
    msg->token = {RecordType::SEGMENT_HEADER, 0};
    msg->d_sgh.filename;
    msg->d_sgh.num_segments = record.num_segments;
    msg->d_sgh.addr = record.addr;
    copyChar(msg->d_sgh.filename, record.filename);
    return true;
}

bool inline RecordWriter::procSegmentHeaderMsg(
        const RecordTypeAndFlags& token,
        const SegmentHeaderInfo& sgh)
{
    bool ret = writeSimpleType(token) && writeString(sgh.filename) && writeVarint(sgh.num_segments)
               && writeSimpleType(sgh.addr);
    return ret;
}

bool inline RecordWriter::writeRecordUnsafe(const ThreadRecord& record)
{
    RecordTypeAndFlags token{RecordType::THREAD_RECORD, 0};
    return writeSimpleType(token) && writeString(record.name);
}

bool inline RecordWriter::writeRecordMsgUnsafe(Msg* msg, const ThreadRecord& record)
{
    msg->token = {RecordType::THREAD_RECORD, 0};
    copyChar(msg->d_thread_name, record.name);
    return true;
}

bool inline RecordWriter::procThreadRecordMsg(const RecordTypeAndFlags& token, const char* thread_name)
{
    bool ret = writeSimpleType(token) && writeString(thread_name);
    return ret;
}

bool inline RecordWriter::writeRecordUnsafe(const UnresolvedNativeFrame& record)
{
    return writeSimpleType(RecordTypeAndFlags{RecordType::NATIVE_TRACE_INDEX, 0})
           && writeIntegralDelta(&d_last.instruction_pointer, record.ip)
           && writeIntegralDelta(&d_last.native_frame_id, record.index);
}

bool inline RecordWriter::writeRecordMsgUnsafe(Msg* msg, const UnresolvedNativeFrame& record)
{
    DebugInfo::write_unresolvednativeframe_msg++;

    msg->token = {RecordType::NATIVE_TRACE_INDEX, 0};
    msg->d_native_frame_record.ip = record.ip;
    msg->d_native_frame_record.index = record.index;

    msg->d_last.instruction_pointer = d_last.instruction_pointer;
    msg->d_last.native_frame_id = d_last.native_frame_id;

    d_last.instruction_pointer = record.ip;
    d_last.native_frame_id = record.index;

    return true;
}

bool inline RecordWriter::procUnresolvedNativeFrameMsg(
        const RecordTypeAndFlags& token,
        const DeltaEncodedFields& last,
        const UnresolvedNativeFrame& record)
{
    bool ret = writeSimpleType(token) && writeIntegralDelta2(last.instruction_pointer, record.ip)
               && writeIntegralDelta2(last.native_frame_id, record.index);
    return ret;
}

bool inline RecordWriter::writeRecordUnsafe(const MemoryMapStart&)
{
    RecordTypeAndFlags token{RecordType::MEMORY_MAP_START, 0};
    return writeSimpleType(token);
}

bool inline RecordWriter::writeRecordMsgUnsafe(Msg* msg, const MemoryMapStart&)
{
    msg->token = {RecordType::MEMORY_MAP_START, 0};
    return true;
}

bool inline RecordWriter::procMemoryMapStartMsg(const RecordTypeAndFlags& token)
{
    return writeSimpleType(token);
}

}  // namespace memray::tracking_api
