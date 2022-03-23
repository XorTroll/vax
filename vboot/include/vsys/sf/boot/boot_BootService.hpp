
#pragma once
#include <switch.h>

namespace vsys::sf::boot {

    Result Initialize();
    void Finalize();

    Result InjectLoader(const u64 addr, u64 &out_start_addr);

}