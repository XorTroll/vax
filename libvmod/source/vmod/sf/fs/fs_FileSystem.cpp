#include <vmod/sf/fs/fs_FileSystem.hpp>
#include <vmod/sf/sf_Service.hpp>

namespace vmod::sf::fs {

    namespace {

        Service g_FspService;

    }

    Result Initialize() {
        if(serviceIsActive(&g_FspService)) {
            return 0;
        }

        VMOD_RC_TRY(GetService(smEncodeName("fsp-srv"), g_FspService));
        // No need to call SetCurrentProcess(...) --> vsys already did itself before sending the service handle, giving us full fs perms ;)
        VMOD_RC_TRY(serviceConvertToDomain(&g_FspService));
        return 0;
    }

    void Finalize() {
        serviceClose(&g_FspService);
    }

    Result OpenSdCardFileSystem(FsFileSystem &out_sd_fs) {
        serviceAssumeDomain(&g_FspService);
        return serviceDispatch(&g_FspService, 18,
            .out_num_objects = 1,
            .out_objects = &out_sd_fs.s
        );
    }

}