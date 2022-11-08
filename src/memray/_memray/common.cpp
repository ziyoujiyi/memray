#include "common.h"

namespace memray {

std::atomic<bool> UserThreadMutex::isActive = false;

}; // namespace memray