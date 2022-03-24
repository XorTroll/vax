
#pragma once
#include <switch.h>

namespace vax::sf::loader {

    Result Initialize();
    void Finalize();

    Result NotifyBootFinished();
    Result LoadModule(const char *mod_path, const size_t mod_path_size, const u64 dest_addr, u64 &out_start_addr, size_t &out_size);

}