
#pragma once
#include <stratosphere.hpp>

#define VSYS_SF_BOOT_SERVICE_INTERFACE_INFO(C, H) \
    AMS_SF_METHOD_INFO(C, H, 0, Result, InjectLoader, (const sf::ClientProcessId &client_pid, sf::CopyHandle &&process_h, const u64 dest_heap_addr, sf::Out<u64> out_start_addr), (client_pid, std::move(process_h), dest_heap_addr, out_start_addr))

AMS_SF_DEFINE_INTERFACE(ams::sf::vsys, IBootService, VSYS_SF_BOOT_SERVICE_INTERFACE_INFO)

namespace vsys::sf {

    class BootService {
        public:
            ams::Result InjectLoader(const ams::sf::ClientProcessId &client_pid, ams::sf::CopyHandle &&process_h, const u64 dest_addr, ams::sf::Out<u64> out_start_addr);
    };
    static_assert(ams::sf::vsys::IsIBootService<BootService>);

}