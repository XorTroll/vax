#include <vax/sf/mod/mod_ModuleService.hpp>
#include <vax/sf/loader/loader_LoaderService.hpp>
#include <vmod/vmod_Assert.hpp>
#include <vmod/vmod_Size.hpp>
#include <vmod/dyn/dyn_Module.hpp>
#include <vmod/sdlog/sdlog_SdCardLogger.hpp>
#include <vmod/vmod_Crt0.hpp>
#include <vmod/vmod_Scope.hpp>
#include <vmod/sf/fs/fs_FileSystem.hpp>
#include <vmod/tlr/tlr_ThreadLocalRegion.hpp>
#include <vector>

extern "C" {

    void _start();

}

namespace {

    vmod::crt0::LoaderContext g_LoaderContext;

    u8 g_TlsTp[0x100] = {};

    std::vector<vmod::dyn::ModuleInfo> g_BuiltinModules;
    std::vector<vmod::dyn::ModuleInfo> g_Modules;

    void RegisterExistingModules() {
        u64 addr = 0;
        while(true) {
            MemoryInfo mem_info;
            u32 page_info;
            const auto rc = svcQueryMemory(&mem_info, &page_info, addr);

            if(mem_info.perm == Perm_Rx) {
                const vmod::dyn::ModuleInfo mod_info = {
                    .base = reinterpret_cast<void*>(mem_info.addr)
                };

                g_Modules.push_back(mod_info);

                if(mod_info.base != reinterpret_cast<void*>(&_start)) {
                    g_BuiltinModules.push_back(mod_info);
                }
            }

            addr = mem_info.addr + mem_info.size;

            if(R_FAILED(rc)) {
                break;
            }
            if(addr == 0) {
                break;
            }
        }
    }

    void RegisterLoadedModule(const u64 base_addr) {
        const vmod::dyn::ModuleInfo mod_info = {
            .base = reinterpret_cast<void*>(base_addr)
        };
        g_Modules.push_back(mod_info);
    }

    void GetCodeStartAddressAndSize(u64 &out_code_start_addr, size_t &out_code_size) {
        u64 addr = 0;
        while(true) {
            MemoryInfo mem_info;
            u32 page_info;
            const auto rc = svcQueryMemory(&mem_info, &page_info, addr);

            if((mem_info.addr != reinterpret_cast<u64>(&_start)) && (mem_info.perm == Perm_Rx)) {
                out_code_start_addr = mem_info.addr;
                out_code_size = mem_info.size;
                return;
            }

            addr = mem_info.addr + mem_info.size;

            if(R_FAILED(rc)) {
                break;
            }
            if(addr == 0) {
                break;
            }
        }
    }

}

extern "C" {

    void __vloader_StartModule(const u64 entry_addr, void *null_arg, vmod::crt0::LoaderContext *loader_ctx);

}

namespace {

    size_t g_ModuleAreaSize = 512_KB;

    inline size_t GetCustomHeapSize() {
        return (g_ModuleAreaSize + 2_MB) & 0xFFE00000;
    }

    u64 UpdateHeap() {
        void *addr;
        const auto size = GetCustomHeapSize();
        VMOD_RC_ASSERT(svcSetHeapSize(&addr, size));

        return reinterpret_cast<u64>(addr);
    }

    u64 FindNextModuleLoadAddress(const u64 cur_heap_addr) {
        auto addr = cur_heap_addr;
        while(true) {
            MemoryInfo mem_info;
            u32 page_info;
            const auto rc = svcQueryMemory(&mem_info, &page_info, addr);

            if(mem_info.perm == Perm_Rw) {
                return mem_info.addr;
            }

            addr = mem_info.addr + mem_info.size;

            if(R_FAILED(rc)) {
                break;
            }
            if(addr == 0) {
                break;
            }
        }
        return 0;
    }

    void LoadModules() {
        u64 program_id;
        VMOD_RC_ASSERT(svcGetInfo(&program_id, InfoType_ProgramId, CUR_PROCESS_HANDLE, 0));

        std::vector<u64> loaded_module_addrs;
        {
            VMOD_RC_ASSERT(vax::sf::loader::Initialize());
            VMOD_ON_SCOPE_EXIT { vax::sf::loader::Finalize(); };

            u64 module_count;
            VMOD_RC_ASSERT(vax::sf::loader::GetProcessModuleCount(module_count));

            for(u64 i = 0; i < module_count; i++) {
                VMOD_SD_LOG_LN("vloader --- loading module %ld!", i);

                const auto cur_heap_addr = UpdateHeap();
                const auto load_addr = FindNextModuleLoadAddress(cur_heap_addr);

                u64 start_addr;
                size_t mod_size;
                VMOD_RC_ASSERT(vax::sf::loader::LoadProcessModule(i, load_addr, start_addr, mod_size));

                RegisterLoadedModule(start_addr);
                loaded_module_addrs.push_back(start_addr);
                g_ModuleAreaSize += mod_size;

                VMOD_SD_LOG_LN("vloader --- loaded module!");
            }
        }

        // Finish setting up the context now that all modules are loaded
        g_LoaderContext.builtin_modules = g_BuiltinModules.data();
        g_LoaderContext.builtin_module_count = g_BuiltinModules.size();
        g_LoaderContext.modules = g_Modules.data();
        g_LoaderContext.module_count = g_Modules.size();

        for(const auto &module_addr: loaded_module_addrs) {
            VMOD_SD_LOG_LN("vloader --- dynlinking module...");
            const vmod::dyn::ModuleInfo mod_info = {
                .base = reinterpret_cast<void*>(module_addr)
            };
            vmod::dyn::DynamicLinkModule(mod_info);

            VMOD_SD_LOG_LN("vloader --- jumping to module...");
            __vloader_StartModule(module_addr, nullptr, &g_LoaderContext);
            
            VMOD_SD_LOG_LN("vloader --- done with module!");
        }
    }

}

namespace {

    /*
        str [sp, #-0x10]!
        svc #0x1
    */
    constexpr u8 SetHeapSizeInvokeCode[] = { 0xE0, 0x0F, 0x1F, 0xF8, 0x21, 0x00, 0x00, 0xD4 };

    /*
        str [sp, #-0x10]!
        svc #0x29
    */
    constexpr u8 GetInfoInvokeCode[] = { 0xE0, 0x0F, 0x1F, 0xF8, 0x21, 0x05, 0x00, 0xD4 };

    /*
        nop
    */
    constexpr u8 NopInstruction[] = { 0x1F, 0x20, 0x03, 0xD5 };

    /*
        ldr x4 #8
        br x4
        adrp x15, #0x1FE03000
        adrp x15, #0x1FE03000
    */
    constexpr u8 PatchInvokeCodeTemplate[] = { 0x44, 0x00, 0x00, 0x58, 0x80, 0x00, 0x1F, 0xD6, 0x0F, 0xF0, 0x0F, 0xF0, 0x0F, 0xF0, 0x0F, 0xF0 };
    using PatchInvokeCodeType = u8[sizeof(PatchInvokeCodeTemplate)];

    template<typename F>
    inline constexpr void MakeSyscallInvokePatch(F intercept_fn, PatchInvokeCodeType &patch_code) {
        __builtin_memcpy(patch_code, PatchInvokeCodeTemplate, sizeof(PatchInvokeCodeTemplate));
        *reinterpret_cast<u64*>(&patch_code[0x8]) = reinterpret_cast<u64>(intercept_fn);
    }

    template<typename F>
    inline Result PatchSyscall(const u64 addr, F intercept_fn) {
        PatchInvokeCodeType patch_code = {};
        MakeSyscallInvokePatch(intercept_fn, patch_code);

        auto copy_addr = addr;
        if(addr & 0x4) {
            VMOD_RC_TRY(vax::sf::mod::MemoryCopy(copy_addr, reinterpret_cast<u64>(NopInstruction), sizeof(NopInstruction)));
            copy_addr += sizeof(NopInstruction);
        }
        VMOD_RC_TRY(vax::sf::mod::MemoryCopy(copy_addr, reinterpret_cast<u64>(patch_code), sizeof(patch_code)));

        return 0;
    }

    namespace svc {

        Result SetHeapSize(void **out_addr, const size_t size) {
            void *addr;
            const auto rc = svcSetHeapSize(&addr, size + GetCustomHeapSize());
            if(R_SUCCEEDED(rc)) {
                const auto actual_addr = reinterpret_cast<u64>(addr) + GetCustomHeapSize();
                *out_addr = reinterpret_cast<void*>(actual_addr);
            }
            return rc;
        }

        Result GetInfo(u64 *out_info, const u64 type, const Handle handle, const u64 subtype) {
            const auto rc = svcGetInfo(out_info, type, handle, subtype);
            if(R_SUCCEEDED(rc) && (type == InfoType_TotalMemorySize) && (subtype == 0) && (handle == CUR_PROCESS_HANDLE)) {
                // todo: maybe moduleareasize?
                const auto actual_mem_size = *out_info - GetCustomHeapSize();
                *out_info = actual_mem_size;
            }
            return rc;
        }

    }

    void PatchHeapSizeSyscalls() {
        VMOD_RC_ASSERT(vax::sf::mod::Initialize());
        VMOD_ON_SCOPE_EXIT { vax::sf::mod::Finalize(); };

        const auto set_heap_size_addr = vmod::dyn::FindCodeAddress(SetHeapSizeInvokeCode, sizeof(SetHeapSizeInvokeCode));
        const auto get_info_addr = vmod::dyn::FindCodeAddress(GetInfoInvokeCode, sizeof(GetInfoInvokeCode));
        
        VMOD_SD_LOG_LN("vloader --- found addrs -> SetHeapSize: 0x%lX, GetInfo: 0x%lX", set_heap_size_addr, get_info_addr);

        if(set_heap_size_addr != 0) {
            VMOD_RC_ASSERT(PatchSyscall(set_heap_size_addr, &svc::SetHeapSize));
            VMOD_SD_LOG_LN("vloader --- patched SetHeapSize!");
        }

        if(get_info_addr != 0) {
            VMOD_RC_ASSERT(PatchSyscall(get_info_addr, &svc::GetInfo));
            VMOD_SD_LOG_LN("vloader --- patched GetInfo!");
        }
    }

}

namespace vmod {

    void OnStartup(vmod::crt0::LoaderContext *&loader_ctx) {
        // vboot sent us the main thread handle in a barebones context
        const auto main_thread_h = loader_ctx->main_thread_h;

        // Set our own tls-tp fake buffer and reent ptr so that they don't point to random game memory after restoring pre-boot code (after boot code is removed)
        // The TLR should already be libnx-style
        auto tlr_ptr = tlr::GetThreadLocalRegion();
        VMOD_ASSERT_TRUE(tlr_ptr->IsLibnxFormat());
        tlr_ptr->libnx_thread_vars.tls_tp = g_TlsTp;
        tlr_ptr->libnx_thread_vars.reent = _impure_ptr;

        // We're the loader, we are the ones who provide the context ;)
        g_LoaderContext.main_thread_h = main_thread_h;

        // Actually provide it for vmod libs
        loader_ctx = &g_LoaderContext;
    }

    void Main() {
        // Initial, partial context setup
        RegisterExistingModules();
        GetCodeStartAddressAndSize(g_LoaderContext.code_start_addr, g_LoaderContext.code_size);

        VMOD_RC_ASSERT(sf::fs::Initialize());
        VMOD_ON_SCOPE_EXIT { sf::fs::Finalize(); };

        FsFileSystem sd_fs;
        VMOD_RC_ASSERT(sf::fs::OpenSdCardFileSystem(sd_fs));
        VMOD_ON_SCOPE_EXIT { fsFsClose(&sd_fs); };

        constexpr const char LogPath[FS_MAX_PATH] = "/vax/vloader.log";
        VMOD_RC_ASSERT(sdlog::Initialize(LogPath, sd_fs));
        VMOD_ON_SCOPE_EXIT { sdlog::Finalize(); };

        VMOD_SD_LOG_LN("vloader --- alive! main thread handle: 0x%X", vmod::crt0::GetLoaderContext()->main_thread_h);

        {
            VMOD_RC_ASSERT(vax::sf::loader::Initialize());
            VMOD_ON_SCOPE_EXIT { vax::sf::loader::Finalize(); };

            VMOD_RC_ASSERT(vax::sf::loader::NotifyBootFinished());
        }
        VMOD_SD_LOG_LN("vloader --- notified vax that vboot finished!");

        PatchHeapSizeSyscalls();
        VMOD_SD_LOG_LN("vloader --- heap-size syscalls patched!");

        LoadModules();
        VMOD_SD_LOG_LN("vloader --- all modules loaded!");

        // Finally, finish with the injection and run the original code
        auto orig_code_addr = reinterpret_cast<void(*)(void*, Handle)>(g_LoaderContext.code_start_addr);
        orig_code_addr(nullptr, g_LoaderContext.main_thread_h);

        // Should be unreachable
        VMOD_RC_ASSERT(0xDEADBEEF);
    }

}