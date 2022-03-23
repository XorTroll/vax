#include <switch.h>
#include <vmod/vmod_Assert.hpp>
#include <vmod/vmod_Size.hpp>
#include <vmod/vmod_Attributes.hpp>
#include <vmod/vmod_Crt0.hpp>
#include <sys/iosupport.h>
#include <vsys/sf/mod/mod_ModuleService.hpp>

extern "C" {

    // TODO: wrap/stub relevant libnx functions to force-avoid using them?

    u32 __nx_applet_type = AppletType_None;
    u32 __nx_fs_num_sessions = 1;

    int main() {}

    VMOD_NORETURN void __nx_exit(Result rc, LoaderReturnFn ret_fn);
    void virtmemSetup();
    void __libc_init_array();
    void __libc_fini_array();

    u8 *fake_heap_start;
    u8 *fake_heap_end;

}

namespace {

    constexpr size_t DefaultHeapSize = 128_KB;

    u8 g_DefaultHeap[DefaultHeapSize];

    LoaderReturnFn g_ReturnFn;
    vmod::crt0::LoaderContext *g_LoaderContext;

}

namespace vmod::crt0 {

    LoaderReturnFn GetReturnFunction() {
        return g_ReturnFn;
    }

    void SetReturnFunction(LoaderReturnFn ret_fn) {
        g_ReturnFn = ret_fn;
    }

    LoaderContext *GetLoaderContext() {
        return g_LoaderContext;
    }

}

namespace vmod {

    VMOD_WEAK void InitializeOwnHeap(u8 *&out_heap_addr, size_t &out_heap_size) {
        out_heap_addr = g_DefaultHeap;
        out_heap_size = sizeof(g_DefaultHeap);
    }

    VMOD_WEAK void OnStartup(vmod::crt0::LoaderContext *&loader_ctx) {}

    extern void Main();

    VMOD_WEAK void OnCleanup() {}

}

extern "C" {

    VMOD_NORETURN void __libnx_init(void *null_arg, vmod::crt0::LoaderContext *loader_ctx, LoaderReturnFn ret_fn) {
        VMOD_ASSERT_TRUE(null_arg == nullptr);
        g_ReturnFn = ret_fn;
        g_LoaderContext = loader_ctx;

        u8 *heap_addr;
        size_t heap_size;
        vmod::InitializeOwnHeap(heap_addr, heap_size);
        fake_heap_start = heap_addr;
        fake_heap_end = heap_addr + heap_size;

        vmod::OnStartup(g_LoaderContext);

        virtmemSetup();
        __libc_init_array();

        // Call plugin entrypoint
        vmod::Main();

        __libc_fini_array();

        vmod::OnCleanup();

        __nx_exit(0, g_ReturnFn);
    }

    void __libnx_exit() {}

    void __appInit() {}
    void __appExit() {}

}