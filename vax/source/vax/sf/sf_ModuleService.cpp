#include <vax/sf/sf_ModuleService.hpp>
#include <vax/log/log_Logger.hpp>

using namespace ams;

namespace vax::sf {

    namespace {

        namespace fsp {

            ams::Result SetCurrentProcess(Service &fsp_srv) {
                u64 pid_placeholder = 0;
                return serviceDispatchIn(&fsp_srv, 1, pid_placeholder, .in_send_pid = true);
            }

        }

    }

    ams::Result ModuleService::GetServiceHandle(const sm::ServiceName &name, ams::sf::OutCopyHandle out_srv_h) {
        VAX_LOG("GetServiceHandle(%s)!", name.name);
        Service srv;
        R_TRY(sm::GetService(&srv, name));

        if(name == sm::ServiceName::Encode("fsp-srv")) {
            // Special case for fsp-srv --> call SetCurrentProcess(pid) to have fs access with our perms (full perms)
            R_TRY(fsp::SetCurrentProcess(srv));
        }

        out_srv_h.SetValue(srv.session, false);
        return ResultSuccess();
    }

    ams::Result ModuleService::MemoryCopy(const ams::sf::ClientProcessId &client_pid, const u64 dst_addr, const u64 src_addr, const size_t size) {
        VAX_LOG("MemoryCopy(dst_addr: 0x%lX, src_addr: 0x%lX, size: 0x%lX)!", dst_addr, src_addr, size);

        svc::Handle debug_h;
        R_TRY(svc::DebugActiveProcess(&debug_h, static_cast<u64>(client_pid.GetValue())));
        ON_SCOPE_EXIT { svc::CloseHandle(debug_h); };

        auto tmp_copy_buf = new u8[size];
        ON_SCOPE_EXIT { delete[] tmp_copy_buf; };
        R_TRY(svc::ReadDebugProcessMemory(reinterpret_cast<uintptr_t>(tmp_copy_buf), debug_h, src_addr, size));
        R_TRY(svc::WriteDebugProcessMemory(debug_h, reinterpret_cast<uintptr_t>(tmp_copy_buf), dst_addr, size));
        return ResultSuccess();
    }

}