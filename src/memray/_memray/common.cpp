#include "common.h"

namespace memray {

std::atomic<bool> UserThreadMutex::isActive = false;
thread_local size_t DebugInfo::total_used_msg_node_num = 0;

}; // namespace memray