#include <vmod/vmod_Assert.hpp>
#include <vmod/vmod_Attributes.hpp>
#include <vmod/vmod_Scope.hpp>
#include <vmod/vmod_Crt0.hpp>
#include <vmod/sdlog/sdlog_SdCardLogger.hpp>
#include <vmod/sf/fs/fs_FileSystem.hpp>
#include <vmod/dyn/dyn_Module.hpp>
#include <vmod/tlr/tlr_ThreadLocalRegion.hpp>
#include <cstring>

// Simple (and still quite ugly) test intercepting flog's global object instance, which contains a lot of info

FsFileSystem g_SdFs;

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

extern "C" {
    extern void _ZN2nn2os15InitializeMutexEPNS0_9MutexTypeEbi(void *a, bool b, int c) VMOD_LINKABLE;
}

bool g_GlobalObjectMemset = false;

void *MemsetHook(void *a, int b, size_t c) {
    if(c == 0x184E0) {
        g_GlobalObjectMemset = true;
    }

    return memset(a, b, c);
}

void MutexHook(void *a, bool b, int c) {
    if(g_GlobalObjectMemset) {
        GlobalObject *global_obj = (GlobalObject*)((u8*)a - 0x33F8);

        // Accessing Nintendo-style TLR's thread handle
        const auto thread_h = vmod::tlr::GetThreadLocalRegion()->thread_ptr->thread_h;

        // fs IPC requires libnx-style TLR since it uses sessionmgr bullshit...
        VMOD_DO_WITH_LIBNX_TLR({
            VMOD_SD_LOG_LN("Hello from in-game! thread handle: 0x%X, fs size: 0x%lX", thread_h, global_obj->fs.fs_size);
        });

        g_GlobalObjectMemset = false;
    }
    
    _ZN2nn2os15InitializeMutexEPNS0_9MutexTypeEbi(a, b, c);
}

namespace vmod {

    void Main() {
        // Note that this is called before the game starts as some sort of preparation-phase
        // Also, not closing fs/sd/sdlog since they will be used later in-game

        VMOD_RC_ASSERT(sf::fs::Initialize());
        // VMOD_ON_SCOPE_EXIT { sf::fs::Finalize(); };

        VMOD_RC_ASSERT(sf::fs::OpenSdCardFileSystem(g_SdFs));
        // VMOD_ON_SCOPE_EXIT { fsFsClose(&g_SdFs); };

        constexpr const char LogPath[FS_MAX_PATH] = "/vtest.log";
        VMOD_RC_ASSERT(sdlog::Initialize(LogPath, g_SdFs));
        // VMOD_ON_SCOPE_EXIT { sdlog::Finalize(); };

        VMOD_SD_LOG_LN("vtest --- alive!");

        VMOD_SD_LOG_LN("vtest --- replacing symbols...");
        dyn::ReplaceSymbol("memset", (void*)&MemsetHook);
        dyn::ReplaceSymbol("_ZN2nn2os15InitializeMutexEPNS0_9MutexTypeEbi", (void*)&MutexHook);        
        VMOD_SD_LOG_LN("vtest --- symbols replaced!");

        VMOD_SD_LOG_LN("vtest --- ending module entrypoint!");
    }

}