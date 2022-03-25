#include <vmod/sdlog/sdlog_SdCardLogger.hpp>
#include <vmod/vmod_Assert.hpp>

namespace vmod::sdlog {

    namespace {

        FsFile g_LogFile;
        u64 g_LogFileOffset;

    }

    Result Initialize(const char (&log_file_path)[FS_MAX_PATH], FsFileSystem &sd_fs) {
        fsFsDeleteFile(&sd_fs, log_file_path);
        fsFsCreateFile(&sd_fs, log_file_path, 0, 0);
        VMOD_RC_TRY(fsFsOpenFile(&sd_fs, log_file_path, FsOpenMode_Write | FsOpenMode_Append, &g_LogFile));

        g_LogFileOffset = 0;
        return 0;
    }

    void Finalize() {
        fsFileClose(&g_LogFile);
    }

    void LogBuffer(const char *buf, const size_t buf_size) {
        VMOD_RC_ASSERT(fsFileWrite(&g_LogFile, g_LogFileOffset, buf, buf_size, FsWriteOption_Flush));
        g_LogFileOffset += buf_size;
    }

}