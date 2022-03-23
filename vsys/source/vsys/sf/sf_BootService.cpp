#include <vsys/sf/sf_BootService.hpp>
#include <vsys/log/log_Logger.hpp>
#include <vsys/elf/elf_Loader.hpp>

using namespace ams;

namespace vsys::sf {

    namespace {

        constexpr const char LoaderModulePath[] = "sdmc:/vloader.elf";

    }

    ams::Result BootService::InjectLoader(const ams::sf::ClientProcessId &client_pid, ams::sf::CopyHandle &&process_h, const u64 dest_heap_addr, ams::sf::Out<u64> out_start_addr) {
        VSYS_LOG("InjectLoader(dest_heap_addr: 0x%lX)!", dest_heap_addr);

        u64 start_addr;
        size_t dummy_size;
        R_TRY(elf::LoadModule(process_h.GetOsHandle(), static_cast<u64>(client_pid.GetValue()), dest_heap_addr, LoaderModulePath, start_addr, dummy_size));

        out_start_addr.SetValue(start_addr);
        return ResultSuccess();
    }

}