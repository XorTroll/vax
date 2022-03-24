#include <vax/log/log_Logger.hpp>

using namespace ams;

namespace vax::log {

    namespace {

        constexpr const char LogFile[] = "sdmc:/vax/vax.log";

        os::Mutex g_LogFileLock(false);
        fs::FileHandle g_LogFile;
        u64 g_LogOffset = 0;

    }

    void Initialize() {
        fs::DeleteFile(LogFile);
        fs::CreateFile(LogFile, 0);
        R_ABORT_UNLESS(fs::OpenFile(&g_LogFile, LogFile, fs::OpenMode_Write | fs::OpenMode_AllowAppend));
    }

    void LogBuffer(const char *buf, const size_t buf_size) {
        std::scoped_lock lk(g_LogFileLock);

        R_ABORT_UNLESS(fs::WriteFile(g_LogFile, g_LogOffset, buf, buf_size, fs::WriteOption::Flush));
        g_LogOffset += buf_size;
    }

}