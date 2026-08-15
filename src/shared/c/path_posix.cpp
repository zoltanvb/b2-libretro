#define _DARWIN_C_SOURCE
#include <shared/system.h>
#include <shared/path.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

#ifdef B2_LIBRETRO_CORE
#include "../../libretro/vfs.h"
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool PathGlob(const std::string &folder,
              std::function<void(const std::string &path,
                                 bool is_folder)>
                  fun) {
#ifdef B2_LIBRETRO_CORE
    if (retro_vfs_glob(folder, fun)) {
        return true;
    }
    // no VFS from the frontend - fall through
#endif

    DIR *d = opendir(folder.c_str());
    if (!d) {
        return -1;
    }

    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        std::string path = PathJoined(folder, de->d_name);

        bool is_folder = PathIsFolderOnDisk(path);

        fun(path, is_folder);
    }

    closedir(d);
    d = NULL;

    return 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool PathIsFileOnDisk(const std::string &path, uint64_t *file_size, bool *can_write) {
#ifdef B2_LIBRETRO_CORE
    if (is_retro_vfs_available()) {
        return retro_vfs_stat(path, file_size, can_write);
    }
#endif

    struct stat st;
    if (stat(path.c_str(), &st) == -1) {
        return false;
    }

    if ((st.st_mode & S_IFREG) != S_IFREG) {
        return false;
    }

    if (file_size) {
        if (st.st_size < 0) {
            *file_size = 0; //???
        } else {
            *file_size = (uint64_t)st.st_size;
        }
    }

    if (can_write) {
        *can_write = access(path.c_str(), W_OK) == 0;
    }

    return true;
}

bool PathIsFolderOnDisk(const std::string &path) {
    struct stat st;
    if (stat(path.c_str(), &st) == -1) {
        return false;
    }

    if ((st.st_mode & S_IFDIR) != S_IFDIR) {
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool PathCreateFolder(const std::string &path) {
#ifdef B2_LIBRETRO_CORE
    if (is_retro_vfs_available()) {
        bool ok = false;
        for (size_t i = 0; i < path.size(); ++i) {
            if (path[i] == '/') {
                ok = retro_vfs_mkdir(path.substr(0, i + 1));
            }
        }
        return ok;
    }
#endif

    int last_rc = -1;
    int last_errno = 0;

    for (size_t i = 0; i < path.size(); ++i) {
        if (path[i] == '/') {
            std::string tmp = path.substr(0, i + 1);

            last_rc = mkdir(tmp.c_str(), 0777);
            last_errno = errno;
        }
    }

    /* Ignore certain errors. */
    if (last_rc == -1 && (last_errno == EEXIST || last_errno == EPERM)) {
        last_rc = 0;
        last_errno = 0;
    }

    errno = last_errno;
    return last_rc == 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
