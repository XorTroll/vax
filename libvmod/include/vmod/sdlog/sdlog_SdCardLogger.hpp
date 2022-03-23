
#pragma once
#include <switch.h>
#include <cstdio>

namespace vmod::sdlog {

    bool Initialize(const char *log_file_path);
    void Finalize();

    void LogBuffer(const char *buf, const size_t buf_size);

}

#define VMOD_SD_LOG_STR(str) ::vmod::sdlog::LogBuffer(str, __builtin_strlen(str))

#define VMOD_SD_LOG_STR_LN(str) ({ \
    VMOD_SD_LOG_STR(str); \
    VMOD_SD_LOG_STR("\n"); \
})

#define VMOD_SD_LOG(fmt, ...) ({ \
    char log_buf[0x200] = {}; \
    const auto log_size = std::snprintf(log_buf, sizeof(log_buf), fmt, ##__VA_ARGS__); \
    ::vmod::sdlog::LogBuffer(log_buf, log_size); \
})

#define VMOD_SD_LOG_LN(fmt, ...) VMOD_SD_LOG(fmt "\n", ##__VA_ARGS__)
