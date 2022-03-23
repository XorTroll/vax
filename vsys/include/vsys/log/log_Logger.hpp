
#pragma once
#include <stratosphere.hpp>

namespace vsys::log {

    void Initialize();
    void LogBuffer(const char *buf, const size_t buf_size);

}

#define VSYS_LOG(fmt, ...) ({ \
    char debug_log[0x200] = {}; \
    const auto debug_log_size = ::ams::util::SNPrintf(debug_log, sizeof(debug_log), fmt "\n", ##__VA_ARGS__); \
    ::vsys::log::LogBuffer(debug_log, debug_log_size); \
})
