
#pragma once
#include <stratosphere.hpp>

namespace vax::sf {

    using Allocator = ams::sf::ExpHeapAllocator;
    using ObjectFactory = ams::sf::ObjectFactory<ams::sf::ExpHeapAllocator::Policy>;

    struct ServerOptions {
        static constexpr size_t PointerBufferSize = 0;
        static constexpr size_t MaxDomains = 0;
        static constexpr size_t MaxDomainObjects = 0;
        static constexpr bool CanDeferInvokeRequest = false;
        static constexpr bool CanManageMitmServers = false;
    };

    enum Port {
        Port_BootService,
        Port_LoaderService,
        Port_ModuleService,

        Port_Count
    };

    constexpr size_t MaxBootServiceSessions = 1;
    constexpr const char BootServiceName[] = "vax:boot";

    constexpr size_t MaxLoaderServiceSessions = 1;
    constexpr const char LoaderServiceName[] = "vax:ldr";

    constexpr size_t MaxModuleServiceSessions = 30;
    constexpr const char ModuleServiceName[] = "vax:mod";

    constexpr size_t MaxSessions = MaxBootServiceSessions + MaxModuleServiceSessions;

    class ServerManager final : public ams::sf::hipc::ServerManager<Port_Count, ServerOptions, MaxSessions> {
        private:
            virtual ams::Result OnNeedsToAccept(int port_index, Server *server) override;
    };

}