#include "romfs.h"

#include <stdint.h>
#include "syscall.h"
#include "path.h"
#include "fs.h"
#include "file.h"
#include "string.h"
#include "object-pool.h"
#include "module.h"
#include "kernel.h"

#define ROMFS_STACK_SIZE 1024

struct romfs_file {
    int fd;
    int device;
    int start;
    size_t len;
};

struct romfs_entry {
    uint32_t parent;
    uint32_t prev;
    uint32_t next;
    uint32_t isdir;
    uint32_t len;
    uint8_t name[PATH_MAX];
};


void romfs_server();
void romfs_module_init();

MODULE_DECLARE(romfs, romfs_module_init);

void romfs_module_init()
{
    int pid;
    struct task_control_block *task;

    pid = kernel_create_task(romfs_server);
    if (pid < 0)
        return;

    task = task_get(pid);
    task_set_priority(task, 2);
    task_set_stack_size(task, ROMFS_STACK_SIZE);
}

int romfs_open_recur(int device, char *path, int this,
                     struct romfs_entry *entry)
{
    if (entry->isdir) {
        /* Iterate through children */
        int pos = this + sizeof(*entry);
        while (pos) {
            /* Get entry */
            lseek(device, pos, SEEK_SET);
            read(device, entry, sizeof(*entry));

            /* Compare path */
            int len = strlen((char *)entry->name);
            if (strncmp((char *)entry->name, path, len) == 0) {
                if (path[len] == '/') { /* Match directory */
                    return romfs_open_recur(device, path + len + 1, pos, entry);
                }
                else if (path[len] == 0) {   /* Match file */
                    return pos;
                }
            }

            /* Next entry */
            pos = entry->next;
        }
    }

    return -1;
}

/*
 * return entry position
 */
int romfs_open(int device, char *path, struct romfs_entry *entry)
{
    /* Get root entry */
    lseek(device, 0, SEEK_SET);
    read(device, entry, sizeof(*entry));

    return romfs_open_recur(device, path, 0, entry);
}

void romfs_server()
{
    DECLARE_OBJECT_POOL(struct romfs_file, files, ROMFS_FILE_LIMIT);
    int self = getpid() + 3;
    struct romfs_entry entry;
    struct fs_request request;
    int cmd;
    int from;
    int device;
    int target;
    int size;
    int pos;
    int status;
    int data_start;
    int data_end;
    char data[REGFILE_BUF];
    struct romfs_file *file;
    struct object_pool_cursor cursor;
    void *addr;

    path_register_fs(ROMFS_TYPE);

    while (1) {
        if (read(self, &request, sizeof(request)) == sizeof(request)) {
            cmd = request.cmd;
            switch (cmd) {
            case FS_CMD_OPEN:
                device = request.device;
                from = request.from;
                pos = request.pos; /* searching starting position */
                pos = romfs_open(request.device, request.path + pos, &entry);

                if (pos >= 0) { /* Found */
                    /* Register */
                    status = path_register(request.path);

                    if (status != -1) {
                        mknod(status, 0, S_IFREG);
                        file = object_pool_allocate(&files);
                        if (file) {
                            file->fd = status;
                            file->device = request.device;
                            file->start = pos + sizeof(entry);
                            file->len = entry.len;
                        }
                    }
                }
                else {
                    status = -1;
                }

                /* Response */
                write(from, &status, sizeof(status));
                break;
            case FS_CMD_READ:
                from = request.from;
                target = request.target;
                size = request.size;
                pos = request.pos;

                /* Find fd */
                object_pool_for_each(&files, cursor, file) {
                    if (file->fd == target) {
                        device = file->device;

                        /* Check boundary */
                        data_start = file->start + pos;
                        if (data_start < file->start) {
                            status = -1;
                            break;
                        }
                        if (data_start > file->start + file->len)
                            data_start = file->start + file->len;

                        data_end = data_start + size;
                        if (data_end > file->start + file->len)
                            data_end = file->start + file->len;
                        break;
                    }
                }
                if (status == -1 || object_pool_cursor_end(&files, cursor)) {
                    write(target, NULL, -1);
                    break;
                }

                /* Get data from device */
                lseek(device, data_start, SEEK_SET);
                size = data_end - data_start;
                size = read(device, data, size);

                /* Response */
                write(target, data, size);
                break;

            case FS_CMD_SEEK:
                target = request.target;
                size = request.size;
                pos = request.pos;

                /* Find fd */
                object_pool_for_each(&files, cursor, file) {
                    if (file->fd == target) {
                        break;
                    }
                }
                if (object_pool_cursor_end(&files, cursor)) {
                    lseek(target, -1, SEEK_SET);
                    break;
                }

                if (pos == 0) { /* SEEK_SET */
                }
                else if (pos < 0) {   /* SEEK_END */
                    size = (file->len) + size;
                }
                else {   /* SEEK_CUR */
                    size = pos + size;
                }
                lseek(target, size, SEEK_SET);
                break;

            case FS_CMD_MMAP:
                target = request.target;
                size = request.size;
                pos = request.pos;

                /* Find fd */
                object_pool_for_each(&files, cursor, file) {
                    if (file->fd == target) {
                        break;
                    }
                }
                if (object_pool_cursor_end(&files, cursor)) {
                    mmap((void *) - 1, 0, 0, 0, -1, 0);
                    break;
                }

                /* Get address from device */
                addr = mmap(0, size, 0, 0, file->device,
                            file->start + pos);

                /* Response */
                mmap(addr, size, 0, 0, target, pos);
                break;


            case FS_CMD_CLOSE:
                target = request.target;

                /* Find fd */
                object_pool_for_each(&files, cursor, file) {
                    if (file->fd == target) {
                        object_pool_free(&files, file);
                        break;
                    }
                }
                break;

            case FS_CMD_WRITE: /* readonly */
            default:
                write(target, NULL, -1);
            }
        }
    }
}
