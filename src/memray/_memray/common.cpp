#include "common.h"

namespace memray {

std::atomic<bool> UserThreadMutex::isActive = false;

thread_local size_t DebugInfo::total_used_msg_node = 0;
thread_local size_t DebugInfo::blocked_cpu_sample = 0;
thread_local size_t DebugInfo::processed_cpu_sample = 0;
thread_local size_t DebugInfo::blocked_allocation = 0;
thread_local size_t DebugInfo::processed_allocation = 0;

thread_local size_t DebugInfo::total_processed_msg = 0;
thread_local size_t DebugInfo::write_unresolvednativeframe_msg = 0;
thread_local size_t DebugInfo::write_frame_push_msg = 0;
thread_local size_t DebugInfo::write_frame_pop_msg = 0;
thread_local size_t DebugInfo::write_allocation_msg = 0;
thread_local size_t DebugInfo::write_native_allocation_msg = 0;
thread_local size_t DebugInfo::write_pyrawframe_msg = 0;
thread_local size_t DebugInfo::write_memory_record_msg = 0;
thread_local size_t DebugInfo::write_segment_msg = 0;
thread_local size_t DebugInfo::write_segment_header_msg = 0;
thread_local size_t DebugInfo::read_unresolvednativeframe_msg = 0;
thread_local size_t DebugInfo::add_cpu_sample = 0;
thread_local size_t DebugInfo::add_allocation = 0;

}; // namespace memray