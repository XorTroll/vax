
#pragma once
#include <switch.h>

namespace vmod::dyn {

    struct ModuleInfo {
        void *base;
    };

    namespace mod {

        struct ModuleStart {
            u8 reserved[4];
            u32 magic_offset;
        };

        struct Header {
            u32 magic;
            u32 dynamic_offset;

            static constexpr u32 Magic = 0x30444F4D;
        };

    }

    void DynamicLinkModule(const ModuleInfo &mod_info);

    u64 FindCodeAddress(const u8 *code_buf, const size_t code_size);
    void ReplaceSymbol(const char *symbol_name, void *new_symbol_fn);

}