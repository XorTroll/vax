
#pragma once
#include <switch.h>

namespace vsys::sf::mod {

    Result Initialize();
    void Finalize();

    Result GetServiceHandle(const SmServiceName name, Handle &out_srv_h);
    Result MemoryCopy(const u64 dst_addr, const u64 src_addr, const size_t size);

}