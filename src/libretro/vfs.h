#ifndef VFS_H
#define VFS_H

#include <string>
#include <functional>
#include <stdint.h>
#include "libretro.h"

#define LIBRETRO_VFS_MAX_SUPPORTED_VERSION 3

void retro_vfs_init(retro_environment_t environ_cb);
bool is_retro_vfs_available(void);

//////////////////////////////////////////////////////////////////////////
// File handle - wraps either a retro_vfs_file_handle* or a stdio FILE*.
//////////////////////////////////////////////////////////////////////////

struct retro_vfs_file;

retro_vfs_file *retro_vfs_fopen(const std::string &path, const char *mode);
void retro_vfs_fclose(retro_vfs_file *f);

size_t retro_vfs_fread(void *ptr, size_t size, size_t nmemb, retro_vfs_file *f);
size_t retro_vfs_fwrite(const void *ptr, size_t size, size_t nmemb, retro_vfs_file *f);

int retro_vfs_fseek(retro_vfs_file *f, int64_t offset, int whence);
int64_t retro_vfs_ftell(retro_vfs_file *f);

int retro_vfs_fgetc(retro_vfs_file *f);
int retro_vfs_fputc(int c, retro_vfs_file *f);

bool retro_vfs_ferror(retro_vfs_file *f);

//////////////////////////////////////////////////////////////////////////
// Misc - stat/mkdir equivalents.
//////////////////////////////////////////////////////////////////////////

bool retro_vfs_stat(const std::string &path, uint64_t *file_size, bool *can_write);
bool retro_vfs_mkdir(const std::string &path);

//////////////////////////////////////////////////////////////////////////
// Directory listing
//////////////////////////////////////////////////////////////////////////

bool retro_vfs_glob(const std::string &folder,
                      std::function<void(const std::string &path, bool is_folder)> fun);

#endif
