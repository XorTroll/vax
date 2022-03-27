#include <vax/sf/sf_IpcManager.hpp>
#include <vax/sf/sf_BootService.hpp>
#include <vax/sf/sf_LoaderService.hpp>
#include <vax/sf/sf_ModuleService.hpp>
#include <vax/mod/mod_Loader.hpp>
#include <vax/log/log_Logger.hpp>

using namespace ams;

namespace vax {

    namespace sf {

        namespace {

            alignas(0x40) constinit u8 g_ManagerAllocatorHeap[8_KB];
            lmem::HeapHandle g_ManagerAllocatorHeapHandle;
            Allocator g_ManagerAllocator;

            ServerManager g_Manager;

            template<typename Impl, typename T, typename ...Args>
            inline auto MakeShared(Args ...args) {
                return ObjectFactory::CreateSharedEmplaced<Impl, T>(std::addressof(g_ManagerAllocator), args...);
            }

            os::ThreadType g_IpcThread;
            alignas(os::ThreadStackAlignment) u8 g_IpcThreadStack[8_KB];

            ams::sf::SharedPointer<ams::sf::vax::IBootService> g_BootServiceInstance;
            ams::sf::SharedPointer<ams::sf::vax::ILoaderService> g_LoaderServiceInstance;
            ams::sf::SharedPointer<ams::sf::vax::IModuleService> g_ModuleServiceInstance;

        }

        ams::Result ServerManager::OnNeedsToAccept(int port_index, Server *server) {
            switch(port_index) {
                case Port_BootService: {
                    return this->AcceptImpl(server, g_BootServiceInstance);
                }
                case Port_LoaderService: {
                    return this->AcceptImpl(server, g_LoaderServiceInstance);
                }
                case Port_ModuleService: {
                    return this->AcceptImpl(server, g_ModuleServiceInstance);
                }
                AMS_UNREACHABLE_DEFAULT_CASE();
            }
        }

        void IpcMain(void*) {
            ams::os::SetThreadNamePointer(ams::os::GetCurrentThread(), "vax.sf.IpcThread");

            svc::Handle boot_srv_port_h;
            R_ABORT_UNLESS(svc::ManageNamedPort(&boot_srv_port_h, BootServiceName, MaxBootServiceSessions));
            g_Manager.RegisterServer(Port_BootService, boot_srv_port_h);

            svc::Handle ldr_srv_port_h;
            R_ABORT_UNLESS(svc::ManageNamedPort(&ldr_srv_port_h, LoaderServiceName, MaxLoaderServiceSessions));
            g_Manager.RegisterServer(Port_LoaderService, ldr_srv_port_h);
            
            svc::Handle mod_srv_port_h;
            R_ABORT_UNLESS(svc::ManageNamedPort(&mod_srv_port_h, ModuleServiceName, MaxModuleServiceSessions));
            g_Manager.RegisterServer(Port_ModuleService, mod_srv_port_h);

            g_Manager.LoopProcess();
        }

        void CreateIpcThread() {
            g_ManagerAllocatorHeapHandle = lmem::CreateExpHeap(g_ManagerAllocatorHeap, sizeof(g_ManagerAllocatorHeap), lmem::CreateOption_None);
            g_ManagerAllocator.Attach(g_ManagerAllocatorHeapHandle);

            g_BootServiceInstance = MakeShared<ams::sf::vax::IBootService, BootService>();
            g_LoaderServiceInstance = MakeShared<ams::sf::vax::ILoaderService, LoaderService>();
            g_ModuleServiceInstance = MakeShared<ams::sf::vax::IModuleService, ModuleService>();

            R_ABORT_UNLESS(ams::os::CreateThread(&g_IpcThread, &IpcMain, nullptr, g_IpcThreadStack, sizeof(g_IpcThreadStack), 10));
            ams::os::StartThread(&g_IpcThread);
        }

    }

}

bool g_InBootProcess = false;

namespace {

    void InjectProcess(const svc::Handle debug_h, const u64 program_id, const u64 thread_id) {
        AMS_UNUSED(program_id);
        u64 boot_start_addr;
        R_ABORT_UNLESS(vax::mod::LoadBootModule(debug_h, boot_start_addr));
        VAX_LOG("Loaded vboot module at 0x%lX", boot_start_addr);

        svc::ThreadContext thread_ctx;
        R_ABORT_UNLESS(svc::GetDebugThreadContext(&thread_ctx, debug_h, thread_id, RegisterGroup_All));

        thread_ctx.pc = boot_start_addr;
        R_ABORT_UNLESS(svc::SetDebugThreadContext(debug_h, thread_id, &thread_ctx, RegisterGroup_All));
    }

    void HandleProcess(const u64 pid) {
        if(g_InBootProcess) {
            VAX_LOG("Another process is already being injected, skipping...");
            return;
        }
        g_InBootProcess = true;

        svc::Handle debug_h;
        R_ABORT_UNLESS(svc::DebugActiveProcess(&debug_h, pid));

        ON_SCOPE_EXIT { svc::CloseHandle(debug_h); };

        svc::lp64::DebugEventInfo debug_event_info;
        u64 program_id = 0;
        while(true) {
            if(R_FAILED(svc::GetDebugEvent(&debug_event_info, debug_h))) {
                break;
            }

            if(debug_event_info.type == svc::DebugEvent_CreateProcess) {
                const auto create_process_info = debug_event_info.info.create_process;

                const auto mod_count = vax::mod::GetProcessModuleCount(create_process_info.program_id);
                if(mod_count > 0) {
                    VAX_LOG("Found %ld modules for process %016lX --- starting injection...", mod_count, create_process_info.program_id);
                    program_id = create_process_info.program_id;
                }
            }
        }

        if(program_id != 0) {
            u64 thread_ids[0x200] = {};
            int thread_id_count;
            do {
                R_ABORT_UNLESS(svc::GetThreadList(&thread_id_count, thread_ids, util::size(thread_ids), debug_h));
                os::SleepThread(TimeSpan::FromMilliSeconds(10));
            } while(thread_id_count == 0);

            InjectProcess(debug_h, program_id, thread_ids[0]);
        }
        else {
            g_InBootProcess = false;
        }
    }

    alignas(ams::os::MemoryPageSize) constinit u8 g_Heap[2_MB];

}

namespace ams {

    namespace init {

        void InitializeSystemModule() {
            init::InitializeAllocator(g_Heap, sizeof(g_Heap));

            R_ABORT_UNLESS(sm::Initialize());
    
            fs::InitializeForSystem();
            R_ABORT_UNLESS(fs::MountSdCard("sdmc"));

            vax::log::Initialize();
        }

        void FinalizeSystemModule() {}

        void Startup() {
            vax::sf::CreateIpcThread();
        }

    }

    void Main() {
        u64 pids[0x200] = {};
        u64 cur_max_pid = 0;
        while(true) {
            int pid_count;
            R_ABORT_UNLESS(svc::GetProcessList(&pid_count, pids, util::size(pids)));

            const auto old_max_pid = cur_max_pid;
            for(int i = 0; i < pid_count; i++) {
                if(pids[i] > cur_max_pid) {
                    cur_max_pid = pids[i];
                }
            }

            if((cur_max_pid != old_max_pid) && (cur_max_pid > 0x80)) {
                HandleProcess(cur_max_pid);
            }

            os::SleepThread(TimeSpan::FromMilliSeconds(10));
        }
    }

}