
#pragma once
#include <switch.h>

namespace vmod::sf::fs {

    Result Initialize();
    void Finalize();

    Result OpenSdCardFileSystem(FsFileSystem &out_sd_fs);

}