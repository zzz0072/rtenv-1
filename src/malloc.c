#include <string.h> /* memset */
#include "malloc.h"
/* Allocate memory:
 * Problems:
 * 1. internal fragment
 * 2. current malloc/free among tasks
 * 3. may refuse to allocte even if pool is availabe due to allocate number
 *    us full */


#define MAX_ALLOCS (128)   /* Max allocate number. Random pick, no reason */
#define POOL_SIZE (1024 * 48)

/* struct to record usage */
struct mem_usage_t {
    char is_used;
    void *addr;
    size_t size;
};

typedef struct mem_usage_t mem_usage;

/* globol pool */
static char g_mem_pool[POOL_SIZE];
static int g_final_slot = 0;
static int g_end_offet  = 0;
static mem_usage g_mem_allocs[MAX_ALLOCS];

void init_mpool(void)
{
    memset(g_mem_allocs, 0x00, sizeof(mem_usage) * MAX_ALLOCS);
}

void *malloc(size_t size)
{
    /* FIXME: Should have a lock here? */
    int i = 0;
    int pad = 0;

    /* Do we need to enlarge size for aligment? */
    pad = size % __BIGGEST_ALIGNMENT__;
    size = size + pad; /* + 0 may better than branch? */

    /* Check total size */
    if (size > POOL_SIZE) {
        return (void *) 0;
    }

    /* If there is available slot? */
    for (i = 0; i < g_final_slot; i++) {
        if (g_mem_allocs[i].is_used == 0 && g_mem_allocs[i].size >= size) {
            g_mem_allocs[i].is_used = 1;
            return g_mem_allocs[i].addr;
        }
    }

    /* try allocate new one */
    if ( g_final_slot == MAX_ALLOCS || g_end_offet + size > POOL_SIZE) {
        return 0;
    }

    g_mem_allocs[g_final_slot].is_used = 1;
    g_mem_allocs[g_final_slot].addr = g_mem_pool + g_end_offet + size;
    g_mem_allocs[g_final_slot].size = size;
    g_end_offet += size;

    return  g_mem_allocs[g_final_slot++].addr;
}

void free(void *ptr)
{
    /* FIXME: Should have a lock here  */
    int i = 0;

    /* Search by addr and free */
    for(i = 0; i < MAX_ALLOCS; i++) {
        if (memcmp(ptr, (void *)&g_mem_allocs[i], sizeof(void *)) == 0) {
            g_mem_allocs[i].is_used = 0;
            return;
        }
    }
}

