#include <vsys/sf/mod/mod_ModuleService.hpp>

namespace vsys::sf::mod {

    namespace {

        Service g_ModuleService;

    }

    Result Initialize() {
        if(serviceIsActive(&g_ModuleService)) {
            return 0;
        }

        Handle srv_h;
        const auto rc = svcConnectToNamedPort(&srv_h, "vsys:mod");
        if(R_SUCCEEDED(rc)) {
            serviceCreate(&g_ModuleService, srv_h);
        }
        return rc;
    }

    void Finalize() {
        serviceClose(&g_ModuleService);
    }

    Result GetServiceHandle(const SmServiceName name, Handle &out_srv_h) {
        return serviceDispatchIn(&g_ModuleService, 0, name,
            .out_handle_attrs = { SfOutHandleAttr_HipcCopy },
            .out_handles = &out_srv_h
        );
    }

    Result MemoryCopy(const u64 dst_addr, const u64 src_addr, const size_t size) {
        const struct {
            u64 pid_placeholder;
            u64 dst_addr;
            u64 src_addr;
            size_t size;
        } in = { 0, dst_addr, src_addr, size };
        return serviceDispatchIn(&g_ModuleService, 1, in, .in_send_pid = true);
    }

}