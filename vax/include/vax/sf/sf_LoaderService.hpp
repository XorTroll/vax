
#pragma once
#include <stratosphere.hpp>

#define VAX_SF_LOADER_SERVICE_INTERFACE_INFO(C, H) \
    AMS_SF_METHOD_INFO(C, H, 0, Result, NotifyBootFinished, (const sf::ClientProcessId &client_pid), (client_pid)) \
    AMS_SF_METHOD_INFO(C, H, 1, Result, LoadModule, (const sf::ClientProcessId &client_pid, sf::CopyHandle &&process_h, const sf::InMapAliasBuffer &mod_path, const u64 dest_addr, sf::Out<u64> out_start_addr, sf::Out<size_t> out_size), (client_pid, std::move(process_h), mod_path, dest_addr, out_start_addr, out_size))

AMS_SF_DEFINE_INTERFACE(ams::sf::vax, ILoaderService, VAX_SF_LOADER_SERVICE_INTERFACE_INFO)

namespace vax::sf {

    class LoaderService {
        public:
            ams::Result NotifyBootFinished(const ams::sf::ClientProcessId &client_pid);
            ams::Result LoadModule(const ams::sf::ClientProcessId &client_pid, ams::sf::CopyHandle &&process_h, const ams::sf::InMapAliasBuffer &mod_path, const u64 dest_addr, ams::sf::Out<u64> out_start_addr, ams::sf::Out<size_t> out_size);
    };
    static_assert(ams::sf::vax::IsILoaderService<LoaderService>);

}