#include <vsys/sf/boot/boot_BootService.hpp>

namespace vsys::sf::boot {

    namespace {

        Service g_BootService;

    }

    Result Initialize() {
        if(serviceIsActive(&g_BootService)) {
            return 0;
        }

        Handle srv_h;
        const auto rc = svcConnectToNamedPort(&srv_h, "vsys:boot");
        if(R_SUCCEEDED(rc)) {
            serviceCreate(&g_BootService, srv_h);
        }
        return rc;
    }

    void Finalize() {
        serviceClose(&g_BootService);
    }

    Result InjectLoader(const u64 addr, u64 &out_start_addr) {
        const struct {
            u64 pid_placeholder;
            u64 addr;
        } in = { 0, addr };
        return serviceDispatchInOut(&g_BootService, 0, in, out_start_addr,
            .in_send_pid = true,
            .in_num_handles = 1,
            .in_handles = {
                CUR_PROCESS_HANDLE
            }
        );
    }

}