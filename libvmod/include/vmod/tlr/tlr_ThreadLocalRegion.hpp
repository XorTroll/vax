
#pragma once
#include <switch.h>
#include <cstring>
#include <sys/iosupport.h>

namespace vmod::tlr {

    constexpr size_t ThreadLocalRegionSize = 0x200;

    struct LibnxThreadVars {
        u32 magic;
        Handle thread_h;
        Thread *thread_ptr;
        _reent *reent;
        void *tls_tp;

        static constexpr u32 Magic = 0x21545624; // '!TV$'
    };
    static_assert(sizeof(LibnxThreadVars) == 0x20);

    // TODO: move this type to proper header
    // TODO: was the thread handle always in this offset? did this struct change over time?
    struct ThreadType {
        u8 data[0x1B8];
        Handle thread_h;
        u8 data_2[0x4];
    };
    static_assert(sizeof(ThreadType) == 0x1C0);

    struct PACKED ThreadLocalRegion {
        u8 ipc_msg_buf[0x100];
        u16 disable_counter;
        u16 interrupt_flag;
        bool in_cache_maintenance;
        union {
            u8 tls_data[0xFB];
            struct PACKED {
                u8 data[0xF3];
                ThreadType *thread_ptr;
            };
            struct PACKED {
                u8 unused_data[ThreadLocalRegionSize - sizeof(LibnxThreadVars) - 0x105];
                LibnxThreadVars libnx_thread_vars;
            };
        };

        inline bool IsLibnxFormat() {
            return this->libnx_thread_vars.magic == LibnxThreadVars::Magic;
        }

        inline bool IsNintendoFormat() {
            return this->thread_ptr != nullptr;
        }

        inline void ConvertToLibnxFormat(u8 *tls_tp_ptr, const Handle thread_h = INVALID_HANDLE) {
            if(this->IsLibnxFormat()) {
                return;
            }
            
            const auto actual_thread_h = this->IsNintendoFormat() ? this->thread_ptr->thread_h : thread_h;

            std::memset(this->tls_data, 0, sizeof(this->tls_data));
            
            this->libnx_thread_vars.magic = LibnxThreadVars::Magic;
            this->libnx_thread_vars.thread_h = actual_thread_h;
            this->libnx_thread_vars.thread_ptr = nullptr;
            this->libnx_thread_vars.reent = _impure_ptr;
            this->libnx_thread_vars.tls_tp = tls_tp_ptr;
        }
    };
    static_assert(sizeof(ThreadLocalRegion) == ThreadLocalRegionSize);
    static_assert(__builtin_offsetof(ThreadLocalRegion, libnx_thread_vars) == ThreadLocalRegionSize - sizeof(LibnxThreadVars));
    static_assert(__builtin_offsetof(ThreadLocalRegion, thread_ptr) == 0x1F8);

    inline ThreadLocalRegion *GetThreadLocalRegion() {
        return reinterpret_cast<ThreadLocalRegion*>(armGetTls());
    }

    inline ThreadLocalRegion BackupThreadLocalRegion() {
        return *GetThreadLocalRegion();
    }

    inline void RestoreThreadLocalRegion(const ThreadLocalRegion &tlr) {
        *GetThreadLocalRegion() = tlr;
    }

    inline void ConvertThreadLocalRegionToLibnxFormat(u8 *tls_tp_ptr, const Handle main_thread_h = INVALID_HANDLE) {
        GetThreadLocalRegion()->ConvertToLibnxFormat(tls_tp_ptr, main_thread_h);
    }

}

#define VMOD_DO_WITH_LIBNX_TLR(...) ({ \
    const auto __backup_tlr = ::vmod::tlr::BackupThreadLocalRegion(); \
    u8 __tmp_tls_tp[0x100] = {}; \
    ::vmod::tlr::ConvertThreadLocalRegionToLibnxFormat(__tmp_tls_tp); \
    __VA_ARGS__ \
    ::vmod::tlr::RestoreThreadLocalRegion(__backup_tlr); \
})
