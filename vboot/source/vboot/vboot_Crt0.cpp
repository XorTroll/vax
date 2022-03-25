#include <vmod/vmod_Assert.hpp>
#include <vmod/vmod_Crt0.hpp>
#include <vmod/tlr/tlr_ThreadLocalRegion.hpp>

extern "C" {

    u32 __nx_fs_num_sessions = 1;

    int main() {}
    int _DYNAMIC;
    void __libnx_init() {}
    void __libnx_exit() {}
    void _init() {}
    void _fini() {}

    NORETURN void __vboot_terminate(void *null_arg, vmod::crt0::LoaderContext *loader_ctx, LoaderReturnFn ret_fn);

}

extern LoaderReturnFn Main(); 

namespace {

    Handle g_MainThreadHandle;
    LoaderReturnFn g_ReturnFn = nullptr;

    vmod::crt0::LoaderContext g_TempContext;

    vmod::tlr::ThreadLocalRegion g_TlrBackup;
    u8 g_TlsTp[0x100] = {};

}

extern "C" {

    void __vboot_init(Handle main_thread_h) {
        g_MainThreadHandle = main_thread_h;

        g_TlrBackup = vmod::tlr::BackupThreadLocalRegion();
        VMOD_ASSERT_TRUE(!g_TlrBackup.IsLibnxFormat());
        VMOD_ASSERT_TRUE(!g_TlrBackup.IsNintendoFormat());
        vmod::tlr::ConvertThreadLocalRegionToLibnxFormat(g_TlsTp, g_MainThreadHandle);
    }

    NORETURN void __vboot_exit() {
        VMOD_ASSERT_TRUE(g_ReturnFn != nullptr);

        // This context is only meant to proviode with the main thread handle
        g_TempContext.main_thread_h = g_MainThreadHandle;

        // vloader gets this barebones context, it will populate its context later
        __vboot_terminate(nullptr, &g_TempContext, g_ReturnFn);
    }

    void __vboot_main() {
        g_ReturnFn = Main();
    }

}