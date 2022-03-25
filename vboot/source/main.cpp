#include <vmod/vmod_Assert.hpp>
#include <vmod/vmod_Size.hpp>
#include <vmod/sdlog/sdlog_SdCardLogger.hpp>
#include <vax/sf/boot/boot_BootService.hpp>
#include <vmod/sf/fs/fs_FileSystem.hpp>
#include <vmod/vmod_Scope.hpp>

using namespace vmod;

namespace {

    // Start with 2MB of base heap to load vloader, then vloader will extend it to load the other modules

    constexpr size_t HeapSize = 2_MB;

}

LoaderReturnFn Main() {
    void *base_heap_addr;
    VMOD_RC_ASSERT(svcSetHeapSize(&base_heap_addr, HeapSize));

    VMOD_RC_ASSERT(vmod::sf::fs::Initialize());
    VMOD_ON_SCOPE_EXIT { vmod::sf::fs::Finalize(); };

    FsFileSystem sd_fs;
    VMOD_RC_ASSERT(vmod::sf::fs::OpenSdCardFileSystem(sd_fs));
    VMOD_ON_SCOPE_EXIT { fsFsClose(&sd_fs); };

    constexpr const char LogPath[FS_MAX_PATH] = "/vax/vboot.log";
    VMOD_RC_ASSERT(vmod::sdlog::Initialize(LogPath, sd_fs));
    VMOD_ON_SCOPE_EXIT { vmod::sdlog::Finalize(); };

    VMOD_SD_LOG_STR_LN("vboot --- alive, extended game heap!");

    LoaderReturnFn ret_fn = nullptr;
    {
        VMOD_RC_ASSERT(vax::sf::boot::Initialize());
        VMOD_ON_SCOPE_EXIT { vax::sf::boot::Finalize(); };

        VMOD_SD_LOG_STR_LN("vboot --- injecting vloader...");

        u64 loader_start_addr;
        VMOD_RC_ASSERT(vax::sf::boot::InjectLoader(reinterpret_cast<u64>(base_heap_addr), loader_start_addr));

        VMOD_SD_LOG_STR_LN("vboot --- injected loader!");

        ret_fn = reinterpret_cast<LoaderReturnFn>(loader_start_addr);
    }

    VMOD_SD_LOG_STR_LN("vboot --- bye!");

    return ret_fn;
}