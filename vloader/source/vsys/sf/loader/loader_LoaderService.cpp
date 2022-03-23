#include <vsys/sf/loader/loader_LoaderService.hpp>

namespace vsys::sf::loader {

    namespace {

        Service g_LoaderService;

    }

    Result Initialize() {
        if(serviceIsActive(&g_LoaderService)) {
            return 0;
        }

        Handle srv_h;
        const auto rc = svcConnectToNamedPort(&srv_h, "vsys:ldr");
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

    Result LoadModule(const char *mod_path, const size_t mod_path_size, const u64 dest_addr, u64 &out_start_addr, size_t &out_size) {
        const struct {
            u64 pid_placeholder;
            u64 dest_addr;
        } in = { 0, dest_addr };
        struct {
            u64 start_addr;
            size_t size;
        } out;

        const auto rc = serviceDispatchInOut(&g_LoaderService, 1, in, out,
            .buffer_attrs = {
                SfBufferAttr_HipcMapAlias | SfBufferAttr_In
            },
            .buffers = {
                { mod_path, mod_path_size }
            },
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