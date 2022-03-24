#include <vax/sf/loader/loader_LoaderService.hpp>

namespace vax::sf::loader {

    namespace {

        Service g_LoaderService;

    }

    Result Initialize() {
        if(serviceIsActive(&g_LoaderService)) {
            return 0;
        }

        Handle srv_h;
        const auto rc = svcConnectToNamedPort(&srv_h, "vax:ldr");
        if(R_SUCCEEDED(rc)) {
            serviceCreate(&g_LoaderService, srv_h);
        }
        return rc;
    }

    void Finalize() {
        serviceClose(&g_LoaderService);
    }

    Result NotifyBootFinished() {
        const u64 pid_placeholder = 0;
        return serviceDispatchIn(&g_LoaderService, 0, pid_placeholder, .in_send_pid = true);
    }

    Result GetProcessModuleCount(u64 &out_count) {
        return serviceDispatchOut(&g_LoaderService, 1, out_count,
            .in_num_handles = 1,
            .in_handles = {
                CUR_PROCESS_HANDLE
            }
        );
    }

    Result LoadProcessModule(const u64 mod_idx, const u64 dest_addr, u64 &out_start_addr, size_t &out_size) {
        const struct {
            u64 pid_placeholder;
            u64 mod_idx;
            u64 dest_addr;
        } in = { 0, mod_idx, dest_addr };
        struct {
            u64 start_addr;
            size_t size;
        } out;

        const auto rc = serviceDispatchInOut(&g_LoaderService, 2, in, out,
            .in_send_pid = true,
            .in_num_handles = 1,
            .in_handles = {
                CUR_PROCESS_HANDLE
            }
        );
        if(R_SUCCEEDED(rc)) {
            out_start_addr = out.start_addr;
            out_size = out.size;
        }

        return rc;
    }

}