
#pragma once
#include <stratosphere.hpp>

#define VAX_SF_MODULE_SERVICE_INTERFACE_INFO(C, H) \
    AMS_SF_METHOD_INFO(C, H, 0, Result, GetServiceHandle, (const sm::ServiceName &name, sf::OutCopyHandle out_srv_h), (name, out_srv_h)) \
    AMS_SF_METHOD_INFO(C, H, 1, Result, MemoryCopy, (const sf::ClientProcessId &client_pid, const u64 dst_addr, const u64 src_addr, const size_t size), (client_pid, dst_addr, src_addr, size))

AMS_SF_DEFINE_INTERFACE(ams::sf::vax, IModuleService, VAX_SF_MODULE_SERVICE_INTERFACE_INFO)

namespace vax::sf {

    class ModuleService {
        public:
            ams::Result GetServiceHandle(const ams::sm::ServiceName &name, ams::sf::OutCopyHandle out_srv_h);
            ams::Result MemoryCopy(const ams::sf::ClientProcessId &client_pid, const u64 dst_addr, const u64 src_addr, const size_t size);
    };
    static_assert(ams::sf::vax::IsIModuleService<ModuleService>);

}