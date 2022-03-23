
#pragma once
#include <switch.h>

namespace vmod::mem {

    Result Copy(const u64 dst_addr, const u64 src_addr, const size_t size);

}