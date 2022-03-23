#include <vmod/vmod_Assert.hpp>
#include <vmod/vmod_Size.hpp>
#include <vmod/sdlog/sdlog_SdCardLogger.hpp>
#include <vsys/sf/boot/boot_BootService.hpp>
#include <vmod/sf/fs/fs_SdCard.hpp>
#include <vmod/vmod_Scope.hpp>

using namespace vmod;

namespace {

    // Start with 2MB of base heap to load vloader, then vloader will extend it to load the other modules

    constexpr size_t HeapSize = 2_MB;

}

LoaderReturnFn Main() {
    void *base_heap_addr;
    VMOD_RC_ASSERT(svcSetHeapSize(&base_heap_addr, HeapSize));

    LoaderReturnFn ret_fn = nullptr;
    {
        VMOD_RC_ASSERT(vsys::sf::boot::Initialize());
        VMOD_ON_SCOPE_EXIT { vsys::sf::boot::Finalize(); };

        u64 loader_start_addr;
        VMOD_RC_ASSERT(vsys::sf::boot::InjectLoader(reinterpret_cast<u64>(base_heap_addr), loader_start_addr));

        ret_fn = reinterpret_cast<LoaderReturnFn>(loader_start_addr);
    }

    return ret_fn;
}