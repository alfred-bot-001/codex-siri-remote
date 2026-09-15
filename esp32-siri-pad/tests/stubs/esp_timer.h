#pragma once
#include <cstdint>
inline uint64_t test_time_ms=1;
inline int64_t esp_timer_get_time(){return test_time_ms*1000;}
