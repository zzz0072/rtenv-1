#include <string.h>
#include "romfs.h"

#include "syscall.h"
#include "path.h"
#include "fs.h"
#include "file.h"
#include "rt_string.h"
#include "rt_dirent.h"
#include "file_metadata.h"

struct romfs_file {
    int fd;
    int device;
    int start;
    size_t len;
};

struct romfs_dirent {
    struct file_metadata_t current_metadata;
    int dir_entry_pos;
    int ref_count;
    int reach_end;
    int is_used;
};

int romfs_open_recur(int device, char *path, int offset, struct file_metadata_t *file_metadata)
{
    if (file_metadata->isdir) {
        /* Iterate through children */
        int curr_pos = offset + sizeof(*file_metadata);
        while (curr_pos) {
            /* Get file_metadata */
            lseek(device, curr_pos, SEEK_SET);
            read(device, file_metadata, sizeof(*file_metadata));

            /* Compare path */
            int len = strlen((char *)file_metadata->name);
            if (strncmp((char *)file_metadata->name, path, len) == 0) {
                if (path[len] == '/') { /* Match directory */
                    return romfs_open_recur(device, path + len + 1, curr_pos, file_metadata);
                }
                else if (path[len] == 0) { /* Match file */
                    return curr_pos;
                }
            }

            /* Next file_metadata */
            curr_pos = file_metadata->next_pos;
        }
    }

    return -1;
}

/*
 * return file_metadata position
 */
int romfs_open(int device, char *path, struct file_metadata_t *file_metadata)
{
    /* Get root file_metadata */
    lseek(device, 0, SEEK_SET);
    read(device, file_metadata, sizeof(*file_metadata));

    return romfs_open_recur(device, path, 0, file_metadata);
}

static int find_avail_DIRS(struct romfs_dirent dir[])
{
    int i = 0;

    for(i = 0; i < ROMFS_FILE_LIMIT; i ++) {
        if(dir[i].is_used == 0) {
            break;
        }
    }

    return i;
}

static int retrieve_next_file_metadata(int device,
                                      struct romfs_dirent *rom_dirent,
                                      struct dirent *dirent)
{
    if (rom_dirent->reach_end) {
        return -1;
    }

    /* Move position to next metadata */
    if (rom_dirent->ref_count == 0) {
        lseek(device, rom_dirent->dir_entry_pos, SEEK_SET);
    }
    else {
        lseek(device, rom_dirent->current_metadata.next_pos, SEEK_SET);
    }

    /* Read next metadata */
    read(device, &(rom_dirent->current_metadata),
            sizeof(struct file_metadata_t));

    if (rom_dirent->current_metadata.next_pos == 0) {
        rom_dirent->reach_end = 1;
    }

    /* Assign metadata to dirent */
    rom_dirent->ref_count++;
    strncpy((char *)dirent->name, (char *)rom_dirent->current_metadata.name,
            PATH_MAX);
    dirent->len = rom_dirent->current_metadata.len;
    dirent->isdir = rom_dirent->current_metadata.isdir;
    return 1;
}

void romfs_server()
{
    struct romfs_file files[ROMFS_FILE_LIMIT];
    struct romfs_dirent DIRS[ROMFS_FILE_LIMIT];
    int nfiles = 0;
    int self = gettid() + 3;
    struct file_metadata_t file_metadata;
    struct fs_request request;
    int cmd;
    int from;
    int device;
    int target;
    int size;
    int pos;
    int status;
    int i;
    int data_start;
    int data_end;
    char data[REGFILE_BUF];

    path_register_fs(ROMFS_TYPE);

    while (1) {
        if (read(self, &request, sizeof(request)) == sizeof(request)) {
            cmd = request.cmd;
            switch (cmd) {
                case FS_CMD_OPEN:
                    device = request.device;
                    from = request.from;
                    pos = request.pos; /* searching starting position */
                    pos = romfs_open(request.device, request.path + pos, &file_metadata);

                    if (pos >= 0) { /* Found */
                        /* Register */
                        status = path_register(request.path);

                        if (status != -1) {
                            mknod(status, 0, S_IFREG);
                            files[nfiles].fd = status;
                            files[nfiles].device = request.device;
                            files[nfiles].start = pos + sizeof(file_metadata);
                            files[nfiles].len = file_metadata.len;
                            nfiles++;
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
                    for (i = 0; i < nfiles; i++) {
                        if (files[i].fd == target) {
                            device = files[i].device;

                            /* Check boundary */
                            data_start = files[i].start + pos;
                            if (data_start < files[i].start) {
                                i = nfiles;
                                break;
                            }
                            if (data_start > files[i].start + files[i].len)
                                data_start = files[i].start + files[i].len;

                            data_end = data_start + size;
                            if (data_end > files[i].start + files[i].len)
                                data_end = files[i].start + files[i].len;
                            break;
                        }
                    }
                    if (i >= nfiles) {
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
                    for (i = 0; i < nfiles; i++) {
                        if (files[i].fd == target) {
                            break;
                        }
                    }
                    if (i >= nfiles) {
                        lseek(target, -1, SEEK_SET);
                        break;
                    }

                    if (pos == 0) { /* SEEK_SET */
                    }
                    else if (pos < 0) { /* SEEK_END */
                        size = (files[i].len) + size;
                    }
                    else { /* SEEK_CUR */
                        size = pos + size;
                    }
                    lseek(target, size, SEEK_SET);
                    break;

                case FS_CMD_STAT:
                    {
                        struct stat fstat = {0};

                        device = request.device;
                        from = request.from;
                        status = 0;
                        pos = request.pos; /* searching starting position */
                        pos = romfs_open(request.device, request.path + pos, &file_metadata);

                        if (pos >= 0) { /* Found */
                            fstat.isdir = file_metadata.isdir;
                            fstat.len = file_metadata.len;
                            strcpy((char *)fstat.name, (char *)file_metadata.name);
                        }
                        else {
                            status = -1;
                        }

                        write(from, &fstat, sizeof(fstat));
                        write(from, &status, sizeof(status));
                    } break;

                case FS_CMD_OPENDIR:
                    {
                        device = request.device;
                        from = request.from;
                        status = -1;

                        /* / is a special case */
                        if (strcmp(request.path, "/") == 0) {
                            /* Get root file_metadata */
                            lseek(device, 0, SEEK_SET);
                            read(device, &file_metadata, sizeof(file_metadata));
                            pos = sizeof(struct file_metadata_t);
                        }
                        else {
                            pos = request.pos; /* searching starting position */
                            pos = romfs_open(request.device,
                                             request.path + pos,
                                             &file_metadata);
                        }

                        /* Found and is a directory */
                        if (pos >= 0 && file_metadata.isdir == 1) {
                            /* Allocate DIRS */
                            int avail_slot = find_avail_DIRS(DIRS);
                            if (avail_slot < ROMFS_FILE_LIMIT) {
                                DIRS[avail_slot].is_used = 1;
                                DIRS[avail_slot].ref_count = 0;
                                DIRS[avail_slot].reach_end = 0;
                                DIRS[avail_slot].dir_entry_pos = pos;
                                DIRS[avail_slot].current_metadata = file_metadata;
                                status = avail_slot;
                            }
                        }

                        write(from, &status, sizeof(status));
                    } break;

                case FS_CMD_CLOSEDIR:
                    {
                        int dir_index = request.pos;
                        from = request.from;
                        status = -1;

                        /* Done only if index is valid */
                        if (dir_index < ROMFS_FILE_LIMIT || 
                                DIRS[dir_index].is_used == 1) {
                            memset((void *)&DIRS[dir_index], 0x00,
                                    sizeof(struct romfs_dirent));
                            status = 0;
                        }

                        write(from, &status, sizeof(status));
                    } break;

                case FS_CMD_READDIR:
                    {
                        int dir_index = request.pos;
                        struct dirent dirent = {0};

                        from = request.from;
                        device = request.device;

                        status = -1;
                        /* Done only if index is valid */
                        if (dir_index < ROMFS_FILE_LIMIT ||
                                DIRS[dir_index].is_used == 1) {
                            status = retrieve_next_file_metadata(device,
                                                              &DIRS[dir_index],
                                                              &dirent);
                        }

                        write(from, &dirent, sizeof(struct dirent));
                        write(from, &status, sizeof(status));
                    } break;


                case FS_CMD_WRITE: /* readonly */
                default:
                    write(target, NULL, -1);
            }
        }
    }
}
