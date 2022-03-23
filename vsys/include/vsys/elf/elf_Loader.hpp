
#pragma once
#include <stratosphere.hpp>

using namespace ams;

namespace vsys::elf {

    ams::Result LoadBootModule(const svc::Handle debug_h, u64 &out_start_addr);
    ams::Result LoadModule(const svc::Handle process_h, const u64 pid, const u64 load_heap_addr, const char *mod_path, u64 &out_start_addr, size_t &out_size);
    ams::Result RestoreBootRegionBackup(const u64 pid);

}