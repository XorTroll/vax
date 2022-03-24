#include <vax/sf/sf_LoaderService.hpp>
#include <vax/log/log_Logger.hpp>
#include <vax/elf/elf_Loader.hpp>

using namespace ams;

namespace vax::sf {

    ams::Result LoaderService::NotifyBootFinished(const ams::sf::ClientProcessId &client_pid) {
        VAX_LOG("NotifyBootFinished()!");

        R_TRY(elf::RestoreBootRegionBackup(static_cast<u64>(client_pid.GetValue())));

        return ResultSuccess();
    }

    ams::Result LoaderService::LoadModule(const ams::sf::ClientProcessId &client_pid, ams::sf::CopyHandle &&process_h, const ams::sf::InMapAliasBuffer &mod_path_buf, const u64 dest_addr, ams::sf::Out<u64> out_start_addr, ams::sf::Out<size_t> out_size) {
        auto mod_path = reinterpret_cast<const char*>(mod_path_buf.GetPointer());
        VAX_LOG("LoadModule(path: '%s', addr: 0x%lX)", mod_path, dest_addr);

        u64 start_addr;
        size_t size;
        R_TRY(elf::LoadModule(process_h.GetOsHandle(), static_cast<u64>(client_pid.GetValue()), dest_addr, mod_path, start_addr, size));

        VAX_LOG("Loaded module!");
        out_start_addr.SetValue(start_addr);
        out_size.SetValue(size);
        return ResultSuccess();
    }

}