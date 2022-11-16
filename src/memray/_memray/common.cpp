#include "common.h"

namespace memray {

std::atomic<bool> PythonStackGuard::isActive = false;

thread_local uint64_t DebugInfo::track_memory_time = 0;
thread_local uint64_t DebugInfo::cpu_handler_time = 0;
thread_local uint64_t DebugInfo::backtrace_time = 0;
thread_local uint64_t DebugInfo::build_call_tree_time = 0;
thread_local uint64_t DebugInfo::dl_open_so_time = 0;
thread_local uint64_t DebugInfo::write_record_msg_time = 0;
thread_local uint64_t DebugInfo::write_threadspecific_record_msg_time = 0;
thread_local uint64_t DebugInfo::prepare_tracker_ins_time = 0;
thread_local uint64_t DebugInfo::proc_record_msg_time = 0;

thread_local size_t DebugInfo::total_used_msg_node = 0;
thread_local size_t DebugInfo::get_avaiable_msg_node_failed = 0;
thread_local size_t DebugInfo::blocked_cpu_sample_dueto_reading = 0;
thread_local size_t DebugInfo::blocked_cpu_sample_dueto_trackingmemory = 0;
thread_local size_t DebugInfo::tracked_cpu_sample = 0;
thread_local size_t DebugInfo::blocked_allocation = 0;
thread_local size_t DebugInfo::tracked_native_allocation = 0;
thread_local size_t DebugInfo::tracked_allocation = 0;
thread_local size_t DebugInfo::tracked_deallocation = 0;

thread_local size_t DebugInfo::total_processed_msg = 0;
thread_local size_t DebugInfo::write_unresolvednativeframe_msg = 0;
thread_local size_t DebugInfo::write_frame_push_msg = 0;
thread_local size_t DebugInfo::write_frame_pop_msg = 0;
thread_local size_t DebugInfo::write_allocation_msg = 0;
thread_local size_t DebugInfo::write_native_allocation_msg = 0;
thread_local size_t DebugInfo::write_pyrawframe_msg = 0;
thread_local size_t DebugInfo::write_memory_record_msg = 0;
thread_local size_t DebugInfo::write_segment = 0;
thread_local size_t DebugInfo::write_segment_header = 0;
thread_local size_t DebugInfo::read_unresolvednativeframe_msg = 0;
thread_local size_t DebugInfo::add_cpu_sample = 0;
thread_local size_t DebugInfo::add_allocation = 0;

};  // namespace memray