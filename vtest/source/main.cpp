#include <vmod/vmod_Assert.hpp>
#include <vmod/vmod_Attributes.hpp>
#include <vmod/vmod_Scope.hpp>
#include <vmod/vmod_Crt0.hpp>
#include <vmod/sdlog/sdlog_SdCardLogger.hpp>
#include <vmod/sf/fs/fs_SdCard.hpp>
#include <vmod/dyn/dyn_Module.hpp>
#include <cstring>

struct FS {
    u8 *fs_start;
    size_t fs_size;
    int file_count;
};
static_assert(sizeof(FS) == 0x18);

struct __attribute__((packed)) GlobalObject {
    u8 data[0x42f8];
    FS fs;
};
static_assert(sizeof(GlobalObject) == 0x42f8 + sizeof(FS));

struct MutexType {
    u8 curState; // _0
    bool isRecursiveMutex; // _1
    s32 lockLevel; // _2
    u8 unk[18];
};
static_assert(sizeof(MutexType) == 28);

extern "C" {
    extern void _ZN2nn2os15InitializeMutexEPNS0_9MutexTypeEbi(MutexType *a, bool b, int c) VMOD_LINKABLE;
    extern void *_ZN2nn2fs6detail8AllocateEm(size_t size) VMOD_LINKABLE;
    extern void _ZN2nn2fs6detail10DeallocateEPvm(void *ptr, size_t size) VMOD_LINKABLE;
}

bool g_GlobalObjectMemset = false;

void *FlogNew(size_t size) {
    return _ZN2nn2fs6detail8AllocateEm(size);
}

void FlogDelete(void *ptr) {
    return _ZN2nn2fs6detail10DeallocateEPvm(ptr, 0);
}

void *MemsetHook(void *a, int b, size_t c) {
    if(c == 0x184E0) {
        g_GlobalObjectMemset = true;
    }

    return memset(a, b, c);
}

/*
void DoWithGlobalObject(GlobalObject *global_obj) {
    FsFile ksd_fs_file;
    ASSERT_RC(fsFsOpenFile(&g_sdfs, "/ksd_fs.bin", FS_OPEN_READ, &ksd_fs_file));

    u64 f_size;
    ASSERT_RC(fsFileGetSize(&ksd_fs_file, &f_size));

    auto ksd_fs_buf = FlogNew(f_size);
    size_t dummy;
    ASSERT_RC(fsFileRead(&ksd_fs_file, 0, ksd_fs_buf, f_size, 0, &dummy));

    fsFileClose(&ksd_fs_file);

    global_obj->fs.fs_start = (u8*)ksd_fs_buf;
    global_obj->fs.fs_size = f_size;
    global_obj->fs.file_count = 1;
}
*/

void MutexHook(MutexType *a, bool b, int c) {
    if(g_GlobalObjectMemset) {
        GlobalObject *global_obj = (GlobalObject*)((u8*)a - 0x33F8);
        svcBreak(0, 0xCAFEDEAF, global_obj->fs.fs_size);
        // DoWithGlobalObject(global_obj);
        g_GlobalObjectMemset = false;
    }
    
    _ZN2nn2os15InitializeMutexEPNS0_9MutexTypeEbi(a, b, c);
}

namespace vmod {

    void Main() {
        VMOD_RC_ASSERT(sf::fs::Initialize());
        VMOD_ON_SCOPE_EXIT { sf::fs::Finalize(); };

        VMOD_RC_ASSERT(sf::fs::MountSdCard());
        VMOD_ON_SCOPE_EXIT { sf::fs::UnmountSdCard(); };

        VMOD_ASSERT_TRUE(sdlog::Initialize("sdmc:/vtest_log.log"));
        VMOD_ON_SCOPE_EXIT { sdlog::Finalize(); };

        VMOD_SD_LOG_LN("vtest --- alive! main thread handle: 0x%X", vmod::crt0::GetLoaderContext()->main_thread_h);
        VMOD_SD_LOG_LN("vtest --- context -> code start: 0x%lX, code size: 0x%lX, builtin module count: %ld, module count: %ld", vmod::crt0::GetLoaderContext()->code_start_addr,
                                                                                                                                    vmod::crt0::GetLoaderContext()->code_size,
                                                                                                                                    vmod::crt0::GetLoaderContext()->builtin_module_count,
                                                                                                                                    vmod::crt0::GetLoaderContext()->module_count);

        VMOD_SD_LOG_LN("vtest --- replacing symbols...");
        dyn::ReplaceSymbol("memset", (void*)&MemsetHook);
        dyn::ReplaceSymbol("_ZN2nn2os15InitializeMutexEPNS0_9MutexTypeEbi", (void*)&MutexHook);        
        VMOD_SD_LOG_LN("vtest --- symbols replaced!");

        VMOD_SD_LOG_LN("vtest --- sayonara!");
    }

}