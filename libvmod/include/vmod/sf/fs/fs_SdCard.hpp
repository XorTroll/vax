
#pragma once
#include <vmod/sf/fs/fs_FileSystem.hpp>

namespace vmod::sf::fs {

    constexpr const char SdCardMountName[] = "sdmc";

    inline Result MountSdCard() {
        FsFileSystem sd_fs;
        VMOD_RC_TRY(OpenSdCardFileSystem(sd_fs));

        fsdevMountDevice(SdCardMountName, sd_fs);
        return 0;
    }

    inline void UnmountSdCard() {
        fsdevUnmountDevice(SdCardMountName);
    }

}