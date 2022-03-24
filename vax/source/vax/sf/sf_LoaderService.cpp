#include <vax/sf/sf_LoaderService.hpp>
#include <vax/log/log_Logger.hpp>
#include <vax/mod/mod_Loader.hpp>

using namespace ams;

namespace vax::sf {

    ams::Result LoaderService::NotifyBootFinished(const ams::sf::ClientProcessId &client_pid) {
        VAX_LOG("NotifyBootFinished()!");

        R_TRY(mod::RestoreBootRegionBackup(static_cast<u64>(client_pid.GetValue())));

        return ResultSuccess();
    }

    ams::Result LoaderService::GetProcessModuleCount(ams::sf::CopyHandle &&process_h, ams::sf::Out<u64> out_count) {
        u64 program_id;
        R_TRY(svc::GetInfo(&program_id, svc::InfoType_ProgramId, process_h.GetOsHandle(), 0));

        VAX_LOG("GetProcessModuleCount(program_id: %016lX)!", program_id);

        out_count.SetValue(mod::GetProcessModuleCount(program_id));
        return ResultSuccess();
    }

    ams::Result LoaderService::LoadProcessModule(const ams::sf::ClientProcessId &client_pid, ams::sf::CopyHandle &&process_h, const u64 mod_idx, const u64 dest_addr, ams::sf::Out<u64> out_start_addr, ams::sf::Out<size_t> out_size) {
        VAX_LOG("LoadProcessModule(mod_idx: %ld, addr: 0x%lX)", mod_idx, dest_addr);

        u64 start_addr;
        size_t size;
        R_TRY(mod::LoadProcessModule(process_h.GetOsHandle(), static_cast<u64>(client_pid.GetValue()), dest_addr, mod_idx, start_addr, size));

        VAX_LOG("Loaded module!");
        out_start_addr.SetValue(start_addr);
        out_size.SetValue(size);
        return ResultSuccess();
    }

}