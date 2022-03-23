#include <vmod/dyn/dyn_Module.hpp>
#include <vmod/vmod_Attributes.hpp>
#include <vmod/vmod_Assert.hpp>
#include <vmod/vmod_Crt0.hpp>
#include <vmod/mem/mem_Memory.hpp>
#include <elf.h>
#include <cstring>
extern "C" {
    #include <ext/ext_BoyerMooreSearch.h>
}

namespace vmod::dyn {

    namespace {

        struct ModuleDynamicInfo {
            ModuleInfo info;
            Elf64_Dyn *dyn;
            Elf64_Rela *rela;
            Elf64_Sym *symtab;
            const char *strtab;
            size_t rela_size;

            inline size_t GetSymbolCount() const {
                return (reinterpret_cast<const u8*>(this->strtab) - reinterpret_cast<u8*>(this->symtab)) / sizeof(Elf64_Sym);
            }
        };

        inline u64 GetCodeStartAddress() {
            return crt0::GetLoaderContext()->code_start_addr;
        }

        inline size_t GetCodeSize() {
            return crt0::GetLoaderContext()->code_size;
        }

        inline size_t GetBuiltinModuleCount() {
            return crt0::GetLoaderContext()->builtin_module_count;
        }

        inline ModuleInfo GetBuiltinModule(const u32 idx) {
            VMOD_ASSERT_TRUE(idx < GetBuiltinModuleCount());
            return crt0::GetLoaderContext()->builtin_modules[idx];
        }

        inline size_t GetModuleCount() {
            return crt0::GetLoaderContext()->module_count;
        }

        inline ModuleInfo GetModule(const u32 idx) {
            VMOD_ASSERT_TRUE(idx < GetModuleCount());
            return crt0::GetLoaderContext()->modules[idx];
        }

        void GetModuleDynamicInfo(const ModuleInfo &mod_info, ModuleDynamicInfo &out_dyn_info) {
            out_dyn_info = {
                .info = mod_info,
                .dyn = nullptr,
                .rela = nullptr,
                .symtab = nullptr,
                .strtab = nullptr,
                .rela_size = 0
            };

            auto mod_base = reinterpret_cast<u8*>(mod_info.base);
            auto mod_start = reinterpret_cast<mod::ModuleStart*>(mod_base);
            auto mod0_header = reinterpret_cast<mod::Header*>(mod_base + mod_start->magic_offset);
            auto dyn = reinterpret_cast<Elf64_Dyn*>(reinterpret_cast<u8*>(mod0_header) + mod0_header->dynamic_offset);
            out_dyn_info.dyn = dyn;

            for(; dyn->d_tag != DT_NULL; dyn++) {
                switch(dyn->d_tag) {
                    case DT_SYMTAB: {
                        out_dyn_info.symtab = reinterpret_cast<Elf64_Sym*>(mod_base + dyn->d_un.d_ptr);
                        break;
                    }
                    case DT_STRTAB: {
                        out_dyn_info.strtab = reinterpret_cast<const char*>(mod_base + dyn->d_un.d_ptr);
                        break;
                    }
                    case DT_RELA: {
                        out_dyn_info.rela = reinterpret_cast<Elf64_Rela*>(mod_base + dyn->d_un.d_ptr);
                        break;
                    }
                    case DT_RELASZ:
                    case DT_PLTRELSZ: {
                        out_dyn_info.rela_size += dyn->d_un.d_val / sizeof(Elf64_Rela);
                        break;
                    }
                    default:
                        break;
                }
            }
        }

        u64 GetModuleSymbolAddress(const ModuleDynamicInfo &mod_dyn_info, const char *symbol_name) {
            for(u32 i = 0; i < mod_dyn_info.GetSymbolCount(); i++) {
                const auto sym = mod_dyn_info.symtab[i];
                if((sym.st_value > 0) && (std::strcmp(mod_dyn_info.strtab + sym.st_name, symbol_name) == 0)) {
                    return reinterpret_cast<u64>(mod_dyn_info.info.base) + sym.st_value;
                }
            }

            return 0;
        }

        inline u64 FindSymbolAddress(const char *symbol_name) {
            for(u32 i = 0; i < GetModuleCount(); i++) {
                const auto mod_info = GetModule(i);
                ModuleDynamicInfo mod_dyn_info;
                GetModuleDynamicInfo(mod_info, mod_dyn_info);

                const auto addr = GetModuleSymbolAddress(mod_dyn_info, symbol_name);
                if(addr != 0) {
                    return addr;
                }
            }

            return 0;
        }

        void ReplaceModuleSymbol(const ModuleInfo &mod_info, const char *symbol_name, void *new_symbol_fn) {
            ModuleDynamicInfo mod_dyn_info;
            GetModuleDynamicInfo(mod_info, mod_dyn_info);

            if((mod_dyn_info.rela == nullptr) || (mod_dyn_info.strtab == nullptr) || (mod_dyn_info.symtab == nullptr)) {
                return;
            }

            const auto sym_count = mod_dyn_info.GetSymbolCount();
            auto cur_rela = mod_dyn_info.rela;
            auto cur_rela_size = mod_dyn_info.rela_size;
            for(u32 rela_idx = 0; cur_rela_size--; cur_rela++, rela_idx++) {
                if(ELF64_R_TYPE(cur_rela->r_info) == R_AARCH64_RELATIVE) {
                    continue;
                }

                const auto sym_idx = ELF64_R_SYM(cur_rela->r_info);
                if(sym_idx >= sym_count) {
                    continue;
                }

                const auto rel_name = mod_dyn_info.strtab + mod_dyn_info.symtab[sym_idx].st_name;
                if(std::strcmp(symbol_name, rel_name) != 0) {
                    continue;
                }

                auto new_rela = *cur_rela;
                new_rela.r_addend += reinterpret_cast<u64>(new_symbol_fn);
                new_rela.r_addend -= FindSymbolAddress(rel_name);
                VMOD_RC_ASSERT(mem::Copy(reinterpret_cast<u64>(cur_rela), reinterpret_cast<u64>(&new_rela), sizeof(new_rela)));
            }
        }

    }

    void DynamicLinkModule(const ModuleInfo &mod_info) {
        ModuleDynamicInfo mod_dyn_info;
        GetModuleDynamicInfo(mod_info, mod_dyn_info);

        if(mod_dyn_info.rela == nullptr) {
            return;
        }

        auto cur_rela = mod_dyn_info.rela;
        auto cur_rela_size = mod_dyn_info.rela_size;
        for(; cur_rela_size--; cur_rela++) {
            if(ELF64_R_TYPE(cur_rela->r_info) == R_AARCH64_RELATIVE) {
                continue;
            }

            const auto sym_idx = ELF64_R_SYM(cur_rela->r_info);
            const auto sym = mod_dyn_info.symtab[sym_idx];
            auto sym_name = mod_dyn_info.strtab + sym.st_name;

            auto sym_val = reinterpret_cast<u64>(mod_info.base) + sym.st_value;
            if(sym.st_value == 0) {
                sym_val = 0;
            }
            if((sym_idx > 0) && (sym.st_shndx == 0)) {
                sym_val = FindSymbolAddress(sym_name);
            }

            switch(ELF64_R_TYPE(cur_rela->r_info)) {
                case R_AARCH64_GLOB_DAT:
                case R_AARCH64_JUMP_SLOT:
                case R_AARCH64_ABS64: {
                    auto ptr = reinterpret_cast<u64*>(reinterpret_cast<u8*>(mod_info.base) + cur_rela->r_offset);
                    *ptr = sym_val + cur_rela->r_addend;
                    break;
                }
                default:
                    break;
            }
        }
    }

    u64 FindCodeAddress(const u8 *code_buf, const size_t code_size) {
        auto addr = GetCodeStartAddress();
        auto size = GetCodeSize();
        while(true) {
            const auto search_out = boyer_moore_search(reinterpret_cast<void*>(addr), size, const_cast<u8*>(code_buf), code_size);
            if(search_out != nullptr) {
                return reinterpret_cast<u64>(search_out);
            }
            addr += size;

            while(true) {
                MemoryInfo mem_info;
                u32 page_info;
                const auto rc = svcQueryMemory(&mem_info, &page_info, addr);

                if((mem_info.perm != Perm_Rx) && (mem_info.perm != Perm_R) && (mem_info.perm != Perm_Rw)) {
                    addr = mem_info.addr + mem_info.size;
                }
                else {
                    addr = mem_info.addr;
                    size = mem_info.size;
                    break;
                }

                if(R_FAILED(rc)) {
                    return 0;
                }
                if(addr == 0) {
                    return 0;
                }
            }
        }
        return 0;
    }

    void ReplaceSymbol(const char *symbol_name, void *new_symbol_fn) {
        for(u32 i = 0; i < GetBuiltinModuleCount(); i++) {
            const auto mod_info = GetBuiltinModule(i);
            ReplaceModuleSymbol(mod_info, symbol_name, new_symbol_fn);
        }
    }

}