#ifndef MALLOC_H_20140225
#define MALLOC_H_20140225
#include <stddef.h>
void init_mpool(void);
void *malloc(size_t size);
void free(void *ptr);
#endif
