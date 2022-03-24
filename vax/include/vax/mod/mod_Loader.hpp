
#pragma once
#include <stratosphere.hpp>

using namespace ams;

namespace vax::mod {

    ams::Result LoadBootModule(const svc::Handle debug_h, u64 &out_start_addr);
    ams::Result RestoreBootRegionBackup(const u64 pid);

    ams::Result LoadModule(const svc::Handle process_h, const u64 pid, const u64 load_heap_addr, const char *mod_path, u64 &out_start_addr, size_t &out_size);
    
    u64 GetProcessModuleCount(const u64 program_id);
    ams::Result LoadProcessModule(const svc::Handle process_h, const u64 pid, const u64 load_heap_addr, const u64 mod_idx, u64 &out_start_addr, size_t &out_size);

}