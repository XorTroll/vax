#include <vmod/mem/mem_Memory.hpp>
#include <vax/sf/mod/mod_ModuleService.hpp>
#include <vmod/vmod_Assert.hpp>
#include <vmod/vmod_Scope.hpp>

namespace vmod::mem {

    Result Copy(const u64 dst_addr, const u64 src_addr, const size_t size) {
        VMOD_RC_TRY(vax::sf::mod::Initialize());
        VMOD_ON_SCOPE_EXIT { vax::sf::mod::Finalize(); };

        VMOD_RC_TRY(vax::sf::mod::MemoryCopy(dst_addr, src_addr, size));
        return 0;
    }

}