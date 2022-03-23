
#pragma once
#include <vsys/sf/mod/mod_ModuleService.hpp>
#include <vmod/vmod_Assert.hpp>
#include <vmod/vmod_Scope.hpp>

namespace vmod::sf {

    inline Result GetService(const SmServiceName name, Service &out_srv) {
        VMOD_RC_TRY(vsys::sf::mod::Initialize());
        VMOD_ON_SCOPE_EXIT { vsys::sf::mod::Finalize(); };

        Handle srv_h;
        const auto rc = vsys::sf::mod::GetServiceHandle(name, srv_h);
        if(R_SUCCEEDED(rc)) {
            serviceCreate(&out_srv, srv_h);
        }
        return rc;
    }

}