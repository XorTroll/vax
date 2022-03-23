#include <vmod/sdlog/sdlog_SdCardLogger.hpp>
#include <vmod/vmod_Assert.hpp>

namespace vmod::sdlog {

    namespace {

        FILE *g_LogFile;

    }

    bool Initialize(const char *log_file_path) {
        g_LogFile = fopen(log_file_path, "wb");
        return g_LogFile != nullptr;
    }

    void Finalize() {
        fclose(g_LogFile);
    }

    void LogBuffer(const char *buf, const size_t buf_size) {
        fwrite(buf, buf_size, 1, g_LogFile);
        fflush(g_LogFile);
    }

}