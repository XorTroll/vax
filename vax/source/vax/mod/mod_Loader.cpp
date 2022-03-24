#include <vax/mod/mod_Loader.hpp>
#include <ext/elf_parser.hpp>
#include <vax/log/log_Logger.hpp>

namespace vax::mod {

    namespace {

        u64 FindMemoryRegion(const svc::Handle debug_h, const u64 min_addr, const size_t size, const svc::MemoryPermission perm) {
            u64 addr = 0;
            while(true) {
                svc::lp64::MemoryInfo mem_info;
                svc::PageInfo page_info;
                const auto rc = svc::QueryDebugProcessMemory(&mem_info, &page_info, debug_h, addr);

                if((mem_info.permission == perm) && (mem_info.base_address >= min_addr) && (mem_info.size >= size) && ((mem_info.state == svc::MemoryState_Code) || (mem_info.state == svc::MemoryState_CodeData))) {
                    return mem_info.base_address;
                }

                addr = mem_info.base_address + mem_info.size;

                if(addr == 0) {
                    break;
                }
                if(rc.IsFailure()) {
                    break;
                }
            }
            return addr;
        }

        ams::Result LoadModuleData(const char *mod_path, u8 *&out_mod_buf, size_t &out_mod_size) {
            fs::FileHandle mod_file;
            R_TRY(fs::OpenFile(&mod_file, mod_path, fs::OpenMode_Read));
            ON_SCOPE_EXIT { fs::CloseFile(mod_file); };

            s64 mod_size;
            R_TRY(fs::GetFileSize(&mod_size, mod_file));

            out_mod_size = mod_size;
            out_mod_buf = new u8[out_mod_size];

            R_TRY(fs::ReadFile(mod_file, 0, out_mod_buf, out_mod_size));

            return ResultSuccess();
        }

        u8 *g_BootModule = nullptr;
        size_t g_BootModuleSize = 0;

        constexpr const char BootModulePath[] = "sdmc:/vax/vboot.elf";

        inline ams::Result EnsureBootModuleLoaded() {
            if(g_BootModule == nullptr) {
                R_TRY(LoadModuleData(BootModulePath, g_BootModule, g_BootModuleSize));
            }

            return ResultSuccess();
        }

        template<u64 Align>
        inline constexpr u64 AlignUp(const u64 v) {
            return (v + (Align - 1)) & ~(Align - 1);
        }

        u8 *g_BootRegionTextBackup = nullptr;
        u64 g_BootRegionTextBackupAddress = 0;
        size_t g_BootRegionTextBackupSize = 0;

        u8 *g_BootRegionDataBackup = nullptr;
        u64 g_BootRegionDataBackupAddress = 0;
        size_t g_BootRegionDataBackupSize = 0;

        inline void MakeProcessModulesPath(char (&out_str)[FS_MAX_PATH], const u64 program_id) {
            std::snprintf(out_str, sizeof(out_str), "sdmc:/atmosphere/contents/%016lX/vax", program_id);
        }

    }

    ams::Result LoadBootModule(const svc::Handle debug_h, u64 &out_start_addr) {
        R_TRY(EnsureBootModuleLoaded());
        elf_parser::Elf_parser parser(g_BootModule);

        const auto segments = parser.get_segments();
        const auto text_segment = segments.at(0);
        const auto text_mem_size = text_segment.phdr->p_memsz;
        const auto text_file_size = text_segment.phdr->p_filesz;
        const auto data_segment = segments.at(1);
        const auto data_mem_size = data_segment.phdr->p_memsz;
        const auto data_file_size = data_segment.phdr->p_filesz;

        // TODO: check addrs ok
        const auto text_addr = FindMemoryRegion(debug_h, 0, text_mem_size, svc::MemoryPermission_ReadExecute);
        const auto data_addr = FindMemoryRegion(debug_h, text_addr, data_file_size, svc::MemoryPermission_ReadWrite);

        parser.relocate_segment(0, text_addr);
        parser.relocate_segment(1, data_addr);

        out_start_addr = text_addr;

        // todo: check this bufs arent already allocated!
        g_BootRegionTextBackupSize = text_mem_size;
        g_BootRegionTextBackupAddress = text_addr;
        g_BootRegionTextBackup = new u8[g_BootRegionTextBackupSize];
        g_BootRegionDataBackupSize = data_mem_size;
        g_BootRegionDataBackupAddress = data_addr;
        g_BootRegionDataBackup = new u8[g_BootRegionDataBackupSize];

        // We're writing the boot module over the process's memory
        // Since the game will run after we load all the modules (which will be loaded in the heap), backup that process memory and restore it later when boot finishes

        R_TRY(svc::ReadDebugProcessMemory(reinterpret_cast<uintptr_t>(g_BootRegionTextBackup), debug_h, text_addr, g_BootRegionTextBackupSize));
        R_TRY(svc::ReadDebugProcessMemory(reinterpret_cast<uintptr_t>(g_BootRegionDataBackup), debug_h, data_addr, g_BootRegionDataBackupSize));

        R_TRY(svc::WriteDebugProcessMemory(debug_h, reinterpret_cast<uintptr_t>(text_segment.data), text_addr, text_file_size));
        R_TRY(svc::WriteDebugProcessMemory(debug_h, reinterpret_cast<uintptr_t>(data_segment.data), data_addr, data_file_size));

        return ResultSuccess();
    }

    ams::Result RestoreBootRegionBackup(const u64 pid) {
        svc::Handle debug_h;
        R_TRY(svc::DebugActiveProcess(&debug_h, pid));
        ON_SCOPE_EXIT { svc::CloseHandle(debug_h); };

        // todo: check that bufs arent null
        R_TRY(svc::WriteDebugProcessMemory(debug_h, reinterpret_cast<uintptr_t>(g_BootRegionTextBackup), g_BootRegionTextBackupAddress, g_BootRegionTextBackupSize));
        R_TRY(svc::WriteDebugProcessMemory(debug_h, reinterpret_cast<uintptr_t>(g_BootRegionDataBackup), g_BootRegionDataBackupAddress, g_BootRegionDataBackupSize));
        
        delete[] g_BootRegionTextBackup;
        g_BootRegionTextBackupAddress = 0;
        g_BootRegionTextBackupSize = 0;

        delete[] g_BootRegionDataBackup;
        g_BootRegionDataBackupAddress = 0;
        g_BootRegionDataBackupSize = 0;

        return ResultSuccess();
    }

    ams::Result LoadModule(const svc::Handle process_h, const u64 pid, const u64 load_heap_addr, const char *mod_path, u64 &out_start_addr, size_t &out_size) {
        u8 *mod_buf;
        size_t mod_size;
        R_TRY(LoadModuleData(mod_path, mod_buf, mod_size));
        ON_SCOPE_EXIT { delete[] mod_buf; };

        elf_parser::Elf_parser parser(mod_buf);
        const auto segments = parser.get_segments();

        // Figure out our number of pages
        u64 min_vaddr = UINT64_MAX;
        u64 max_vaddr = 0;
        for(const auto &seg : segments) {
            const auto min = seg.phdr->p_vaddr;
            const auto max = min + AlignUp<0x1000>(seg.phdr->p_memsz);

            if(min < min_vaddr) {
                min_vaddr = min;
            }

            if(max > max_vaddr) {
                max_vaddr = max;
            }
        }

        VAX_LOG("Pre debug write -> min: 0x%lX, max: 0x%lX", min_vaddr, max_vaddr);

        // Debug the process to write into the heap addr provided
        {
            svc::Handle debug_h;
            R_TRY(svc::DebugActiveProcess(&debug_h, pid));
            ON_SCOPE_EXIT { svc::CloseHandle(debug_h); };

            for(const auto &seg : segments) {
                R_TRY(svc::WriteDebugProcessMemory(debug_h, reinterpret_cast<uintptr_t>(seg.data), load_heap_addr + seg.phdr->p_vaddr, seg.phdr->p_filesz));
            }
        }

        VAX_LOG("Post debug write");

        const auto size = max_vaddr - min_vaddr;
        // Unmap heap and map new code
        u64 load_addr;
        while(true) {
            load_addr = randomGet64() & 0xFFFFFF000ull;
            R_TRY_CATCH(svc::MapProcessCodeMemory(process_h, load_addr, load_heap_addr, size)) {
                R_CATCH(svc::ResultInvalidCurrentMemory, svc::ResultInvalidMemoryRegion) {
                    continue;
                }
            } R_END_TRY_CATCH;
            break;
        }

        VAX_LOG("Load addr: 0x%lX", load_addr);

        for(const auto &seg : segments) {
            u32 seg_perms = svc::MemoryPermission_None;
            for(const auto &f : seg.segment_flags) {
                switch(f) {
                    case 'R': {
                        seg_perms |= svc::MemoryPermission_Read;
                        break;
                    }
                    case 'W': {
                        seg_perms |= svc::MemoryPermission_Write;
                        break;
                    }
                    case 'E': {
                        seg_perms |= svc::MemoryPermission_Execute;
                        break;
                    }
                }
            }
            svc::SetProcessMemoryPermission(process_h, load_addr + seg.phdr->p_vaddr, AlignUp<0x1000>(seg.phdr->p_memsz), static_cast<svc::MemoryPermission>(seg_perms));
        }

        out_start_addr = load_addr;
        out_size = size;

        return ResultSuccess();
    }

    u64 GetProcessModuleCount(const u64 program_id) {
        char modules_dir_path[FS_MAX_PATH];
        MakeProcessModulesPath(modules_dir_path, program_id);

        fs::DirectoryHandle modules_dir;
        if(R_SUCCEEDED(fs::OpenDirectory(&modules_dir, modules_dir_path, fs::OpenDirectoryMode_File))) {
            ON_SCOPE_EXIT { fs::CloseDirectory(modules_dir); };

            s64 entry_file_count;
            fs::GetDirectoryEntryCount(&entry_file_count, modules_dir);
            return entry_file_count;
        }

        return 0;
    }

    ams::Result LoadProcessModule(const svc::Handle process_h, const u64 pid, const u64 load_heap_addr, const u64 mod_idx, u64 &out_start_addr, size_t &out_size) {
        u64 program_id;
        R_TRY(svc::GetInfo(&program_id, svc::InfoType_ProgramId, process_h, 0));

        char modules_dir_path[FS_MAX_PATH] = {};
        MakeProcessModulesPath(modules_dir_path, program_id);

        fs::DirectoryHandle modules_dir;
        R_TRY(fs::OpenDirectory(&modules_dir, modules_dir_path, fs::OpenDirectoryMode_File));
        ON_SCOPE_EXIT { fs::CloseDirectory(modules_dir); };

        u64 idx = 0;
        while(true) {
            s64 dummy_count;
            fs::DirectoryEntry entry;
            R_TRY(fs::ReadDirectory(&dummy_count, &entry, modules_dir, 1));
            if(dummy_count != 1) {
                break;
            }

            if(idx == mod_idx) {
                char module_path[FS_MAX_PATH] = {};
                std::snprintf(module_path, sizeof(module_path), "%s/%s", modules_dir_path, entry.name);

                R_TRY(LoadModule(process_h, pid, load_heap_addr, module_path, out_start_addr, out_size));
                break;
            }

            idx++;
        }

        return ResultSuccess();
    }

}