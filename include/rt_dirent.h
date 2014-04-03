#ifndef DIRENT_H_201404
#define DIRENT_H_201404
#include <stdint.h>

struct dirent {
    uint32_t isdir;
    uint32_t len;
    uint8_t name[PATH_MAX];
};

typedef int RT_DIR;

RT_DIR opendir(const char *name);
int closedir(RT_DIR dir);
#endif
