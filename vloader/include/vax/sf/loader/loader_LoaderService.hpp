
#pragma once
#include <switch.h>

namespace vax::sf::loader {

    Result Initialize();
    void Finalize();

    Result NotifyBootFinished();
    Result GetProcessModuleCount(u64 &out_count);
    Result LoadProcessModule(const u64 mod_idx, const u64 dest_addr, u64 &out_start_addr, size_t &out_size);

}