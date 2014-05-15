#ifndef FILE_METADATA_H_20140316
#define FILE_METADATA_H_20140316
#include <stdint.h>

#ifdef BUILD_HOST
    #undef PIPE_BUF
    #undef PATH_MAX
#endif

#include "kconfig.h"
struct file_metadata_t {
    uint32_t parent_pos;
    uint32_t prev_pos;
    uint32_t next_pos;
    uint32_t isdir;
    uint32_t len;
    uint8_t name[PATH_MAX];
};

#endif

