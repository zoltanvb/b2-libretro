#include "vfs.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>

#if defined(_WIN32)
#include <direct.h>
#define B2_MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#include <sys/types.h>
#define B2_MKDIR(path) mkdir(path, 0777)
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static struct retro_vfs_interface *g_vfs_interface = nullptr;
static uint32_t g_vfs_interface_version = 0;

void retro_vfs_init(retro_environment_t environ_cb) {

    // already initialized
    if (g_vfs_interface != nullptr)
        return;

    if (!environ_cb) {
        return;
    }

    struct retro_vfs_interface_info vfs_info;
    vfs_info.required_interface_version = LIBRETRO_VFS_MAX_SUPPORTED_VERSION;
    vfs_info.iface = nullptr;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VFS_INTERFACE, &vfs_info) && vfs_info.iface) {
        g_vfs_interface = vfs_info.iface;
        g_vfs_interface_version = vfs_info.required_interface_version;
    }
}

bool is_retro_vfs_available(void) {
    return g_vfs_interface != nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct retro_vfs_file {
    bool via_vfs = false;
    struct retro_vfs_file_handle *vfs_handle = nullptr;
    FILE *stdio_handle = nullptr;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static unsigned get_vfs_open_flags(const char *mode) {
    unsigned flags = 0;

    bool has_plus = strchr(mode, '+') != nullptr;
    bool has_w = strchr(mode, 'w') != nullptr;
    bool has_a = strchr(mode, 'a') != nullptr;

    if (has_w || has_a) {
        flags |= RETRO_VFS_FILE_ACCESS_WRITE;
        if (has_plus) {
            flags |= RETRO_VFS_FILE_ACCESS_READ;
        }
        if (has_a) {
            flags |= RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING;
        }
    } else if (has_plus) {
        // "r+b" - read/write, file must already exist.
        flags |= RETRO_VFS_FILE_ACCESS_READ | RETRO_VFS_FILE_ACCESS_WRITE | RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING;
    } else {
        flags |= RETRO_VFS_FILE_ACCESS_READ;
    }

    return flags;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

retro_vfs_file *retro_vfs_fopen(const std::string &path, const char *mode) {
    retro_vfs_file *f = new retro_vfs_file();

    if (g_vfs_interface && g_vfs_interface->open) {
        unsigned flags = get_vfs_open_flags(mode);

        f->vfs_handle = g_vfs_interface->open(path.c_str(), flags, RETRO_VFS_FILE_ACCESS_HINT_NONE);
        if (!f->vfs_handle) {
            delete f;
            return nullptr;
        }

        f->via_vfs = true;
        return f;
    }

    // fallback no VFS
    f->stdio_handle = fopen(path.c_str(), mode);
    if (!f->stdio_handle) {
        delete f;
        return nullptr;
    }

    f->via_vfs = false;
    return f;
}

void retro_vfs_fclose(retro_vfs_file *f) {
    if (!f) {
        return;
    }

    if (f->via_vfs) {
        if (g_vfs_interface && g_vfs_interface->close && f->vfs_handle) {
            g_vfs_interface->close(f->vfs_handle);
        }
    } else if (f->stdio_handle) {
        fclose(f->stdio_handle);
    }

    delete f;
}

size_t retro_vfs_fread(void *ptr, size_t size, size_t nmemb, retro_vfs_file *f) {
    if (!f || size == 0 || nmemb == 0) {
        return 0;
    }

    if (f->via_vfs) {
        if (!g_vfs_interface || !g_vfs_interface->read) {
            return 0;
        }

        int64_t n = g_vfs_interface->read(f->vfs_handle, ptr, (uint64_t)size * nmemb);
        if (n < 0) {
            return 0;
        }

        return (size_t)n / size;
    }

    return fread(ptr, size, nmemb, f->stdio_handle);
}

size_t retro_vfs_fwrite(const void *ptr, size_t size, size_t nmemb, retro_vfs_file *f) {
    if (!f || size == 0 || nmemb == 0) {
        return 0;
    }

    if (f->via_vfs) {
        if (!g_vfs_interface || !g_vfs_interface->write) {
            return 0;
        }

        int64_t n = g_vfs_interface->write(f->vfs_handle, ptr, (uint64_t)size * nmemb);
        if (n < 0) {
            return 0;
        }

        return (size_t)n / size;
    }

    return fwrite(ptr, size, nmemb, f->stdio_handle);
}

int retro_vfs_fseek(retro_vfs_file *f, int64_t offset, int whence) {
    if (!f) {
        return -1;
    }

    if (f->via_vfs) {
        if (!g_vfs_interface || !g_vfs_interface->seek) {
            return -1;
        }

        int seek_position;
        switch (whence) {
        default:
        case SEEK_SET:
            seek_position = RETRO_VFS_SEEK_POSITION_START;
            break;

        case SEEK_CUR:
            seek_position = RETRO_VFS_SEEK_POSITION_CURRENT;
            break;

        case SEEK_END:
            seek_position = RETRO_VFS_SEEK_POSITION_END;
            break;
        }

        return g_vfs_interface->seek(f->vfs_handle, offset, seek_position) < 0 ? -1 : 0;
    }

    return fseek(f->stdio_handle, (long)offset, whence);
}

int64_t retro_vfs_ftell(retro_vfs_file *f) {
    if (!f) {
        return -1;
    }

    if (f->via_vfs) {
        if (!g_vfs_interface || !g_vfs_interface->tell) {
            return -1;
        }

        return g_vfs_interface->tell(f->vfs_handle);
    }

    return ftell(f->stdio_handle);
}

int retro_vfs_fgetc(retro_vfs_file *f) {
    if (!f) {
        return EOF;
    }

    if (f->via_vfs) {
        unsigned char c;
        if (retro_vfs_fread(&c, 1, 1, f) != 1) {
            return EOF;
        }
        return c;
    }

    return fgetc(f->stdio_handle);
}

int retro_vfs_fputc(int c, retro_vfs_file *f) {
    if (!f) {
        return EOF;
    }

    if (f->via_vfs) {
        unsigned char b = (unsigned char)c;
        if (retro_vfs_fwrite(&b, 1, 1, f) != 1) {
            return EOF;
        }
        return b;
    }

    return fputc(c, f->stdio_handle);
}

bool retro_vfs_ferror(retro_vfs_file *f) {
    if (!f) {
        return true;
    }

    if (f->via_vfs) {
        return f->vfs_handle == nullptr;
    }

    return ferror(f->stdio_handle) != 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool retro_vfs_stat(const std::string &path, uint64_t *file_size, bool *can_write) {
    if (g_vfs_interface && g_vfs_interface->stat && g_vfs_interface_version >= 3) {
        int32_t size = 0;
        int result = g_vfs_interface->stat(path.c_str(), &size);

        if (!(result & RETRO_VFS_STAT_IS_VALID)) {
            return false;
        }

        if (result & RETRO_VFS_STAT_IS_DIRECTORY) {
            return false;
        }

        if (file_size) {
            *file_size = size < 0 ? 0 : (uint64_t)size;
        }

        if (can_write) {
            *can_write = true;
        }

        return true;
    }

    // fallback - no VFS
    FILE *fp = fopen(path.c_str(), "rb");
    if (!fp) {
        return false;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return false;
    }

    long size = ftell(fp);
    fclose(fp);

    if (size < 0) {
        return false;
    }

    if (file_size) {
        *file_size = (uint64_t)size;
    }

    if (can_write) {
        FILE *wfp = fopen(path.c_str(), "r+b");
        *can_write = wfp != nullptr;
        if (wfp) {
            fclose(wfp);
        }
    }

    return true;
}

bool retro_vfs_mkdir(const std::string &path) {
    if (g_vfs_interface && g_vfs_interface->mkdir && g_vfs_interface_version >= 3) {
        int result = g_vfs_interface->mkdir(path.c_str());
        // 0 = success, -2 = already exists - both fine.
        return result == 0 || result == -2;
    }

    if (B2_MKDIR(path.c_str()) == 0) {
        return true;
    }

    return errno == EEXIST;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool retro_vfs_glob(const std::string &folder,
                      std::function<void(const std::string &path, bool is_folder)> fun) {
    if (g_vfs_interface && g_vfs_interface->opendir && g_vfs_interface_version >= 3) {
        struct retro_vfs_dir_handle *dir = g_vfs_interface->opendir(folder.c_str(), true);
        if (!dir) {
            return false;
        }

        while (g_vfs_interface->readdir(dir)) {
            const char *name = g_vfs_interface->dirent_get_name(dir);
            if (!name) {
                continue;
            }

            std::string path = folder;
            if (!path.empty() && path.back() != '/' && path.back() != '\\') {
                path += "/";
            }
            path += name;

            fun(path, g_vfs_interface->dirent_is_dir(dir));
        }

        g_vfs_interface->closedir(dir);
        return true;
    }

    return false;
}
