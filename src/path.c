#include "path.h"

#include "file.h"
#include "kconfig.h"
#include "rt_string.h"
#include "syscall.h"
#include "fs.h"

struct mount {
    int fs;
    int dev;
    char path[PATH_MAX];
};

/*
 * pathserver assumes that all files are FIFOs that were registered
 * with mkfifo.  It also assumes a global tables of FDs shared by all
 * processes.  It would have to get much smarter to be generally useful.
 *
 * The first TASK_LIMIT FDs are reserved for use by their respective tasks.
 * 0-2 are reserved FDs and are skipped.
 * The server registers itself at /sys/pathserver
 */
void pathserver()
{
    char paths[FILE_LIMIT - TASK_LIMIT - 3][PATH_MAX];
    int npaths = 0;
    int fs_fds[FS_LIMIT];
    char fs_types[FS_LIMIT][FS_TYPE_MAX];
    int nfs_types = 0;
    struct mount mounts[MOUNT_LIMIT];
    int nmounts = 0;
    int i = 0;
    int cmd = 0;
    size_t path_len = 0;
    unsigned int replyfd = 0;
    char path[PATH_MAX];
    int dev = 0;
    int newfd = 0;
    char fs_type[FS_TYPE_MAX];
    int status;

    memcpy(paths[npaths++], PATH_SERVER_NAME, sizeof(PATH_SERVER_NAME));

    while (1) {
        read(PATHSERVER_FD, &cmd, INT_SIZE);
        read(PATHSERVER_FD, &replyfd, INT_SIZE);

        switch (cmd) {
            case PATH_CMD_MKFILE:
                read(PATHSERVER_FD, &path_len, SIZE_T_SIZE);
                read(PATHSERVER_FD, path, path_len);
                read(PATHSERVER_FD, &dev, INT_SIZE);
                newfd = npaths + 3 + TASK_LIMIT;
                if (mknod(newfd, 0, dev) == 0) {
                    memcpy(paths[npaths], path, path_len);
                    npaths++;
                }
                else {
                    newfd = -1;
                }
                write(replyfd, &newfd, INT_SIZE);
                break;

            case PATH_CMD_OPEN:
                read(PATHSERVER_FD, &path_len, SIZE_T_SIZE);
                read(PATHSERVER_FD, path, path_len);

                /* Search files in paths[] first path is assigned while
                 * mkfifo or path_register was called */
                for (i = 0; i < npaths; i++) {
                    if (*paths[i] && strcmp(path, paths[i]) == 0) {
                        i += 3; /* 0-2 are reserved */
                        i += TASK_LIMIT; /* FDs reserved for tasks */
                        write(replyfd, &i, INT_SIZE);
                        i = 0;
                        break;
                    }
                }

                if (i < npaths) {
                    break;
                }

                /* Search for mount point if its no opened romfs file,
                 * or FIFO file with mkfifo.
                 *
                 * This is done by sending FS_CMD_OPEN request.
                 * path_register will be called eventually */
                for (i = 0; i < nmounts; i++) {
                    if (*mounts[i].path
                            && strncmp(path, mounts[i].path,
                                       strlen(mounts[i].path)) == 0) {
                        int mlen = strlen(mounts[i].path);
                        struct fs_request request;
                        request.cmd = FS_CMD_OPEN;
                        request.from = replyfd;
                        request.device = mounts[i].dev;
                        request.pos = mlen; /* search starting position */
                        memcpy(request.path, &path, path_len);
                        write(mounts[i].fs, &request, sizeof(request));
                        i = 0;
                        break;
                    }
                }

                if (i >= nmounts) {
                    i = -1; /* Error: not found */
                    write(replyfd, &i, INT_SIZE);
                }
                break;

            case PATH_CMD_STAT:
                {
                    struct stat fstat = {0};

                    status = 0;
                    /* Get file name */
                    read(PATHSERVER_FD, &path_len, SIZE_T_SIZE);
                    read(PATHSERVER_FD, path, path_len);

                    /* Get file stat, response was sent by mounted fs */
                    for (i = 0; i < nmounts; i++) {
                        if (*mounts[i].path
                                && strncmp(path, mounts[i].path,
                                           strlen(mounts[i].path)) == 0) {
                            int mlen = strlen(mounts[i].path);
                            struct fs_request request;
                            request.cmd = FS_CMD_STAT;
                            request.from = replyfd;
                            request.device = mounts[i].dev;
                            request.pos = mlen; /* search starting position */
                            memcpy(request.path, &path, path_len);
                            write(mounts[i].fs, &request, sizeof(request));
                            break;
                        }
                    }

                    /* Not found */
                    if (i >= nmounts) {
                        status = -1;
                        write(replyfd, &fstat, sizeof(fstat));
                        write(replyfd, &status, INT_SIZE);
                    }
                } break;

            case PATH_CMD_REGISTER_PATH:
                read(PATHSERVER_FD, &path_len, SIZE_T_SIZE);
                read(PATHSERVER_FD, path, path_len);
                newfd = npaths + 3 + TASK_LIMIT;
                memcpy(paths[npaths], path, path_len);
                npaths++;
                write(replyfd, &newfd, INT_SIZE);
                break;

            case PATH_CMD_REGISTER_FS:
                read(PATHSERVER_FD, &path_len, SIZE_T_SIZE);
                read(PATHSERVER_FD, fs_type, path_len);
                fs_fds[nfs_types] = replyfd;
                memcpy(fs_types[nfs_types], fs_type, path_len);
                nfs_types++;
                i = 0;
                write(replyfd, &i, INT_SIZE);
                break;

            case PATH_CMD_MOUNT: {
                size_t slen;
                size_t dlen;
                size_t tlen;
                char src[PATH_MAX];
                char dst[PATH_MAX];
                char type[FS_TYPE_MAX];
                read(PATHSERVER_FD, &slen, SIZE_T_SIZE);
                read(PATHSERVER_FD, src, slen);
                read(PATHSERVER_FD, &dlen, SIZE_T_SIZE);
                read(PATHSERVER_FD, dst, dlen);
                read(PATHSERVER_FD, &tlen, SIZE_T_SIZE);
                read(PATHSERVER_FD, type, tlen);

                /* Search for filesystem types */
                for (i = 0; i < nfs_types; i++) {
                    if (*fs_types[i] && strcmp(type, fs_types[i]) == 0) {
                        break;
                    }
                }

                if (i >= nfs_types) {
                    status = -1; /* Error: not found */
                    write(replyfd, &status, INT_SIZE);
                    break;
                }

                mounts[nmounts].fs = fs_fds[i];

                /* Search for device */
                for (i = 0; i < npaths; i++) {
                    if (*paths[i] && strcmp(src, paths[i]) == 0) {
                        break;
                    }
                }

                if (i >= npaths) {
                    status = -1; /* Error: not found */
                    write(replyfd, &status, INT_SIZE);
                    break;
                }

                /* Store mount point */
                mounts[nmounts].dev = i + 3 + TASK_LIMIT;
                memcpy(mounts[nmounts].path, dst, dlen);
                nmounts++;

                status = 0;
                write(replyfd, &status, INT_SIZE);
            }   break;

            default:
                ;
        }
    }
}

int path_register(const char *pathname)
{
    int cmd = PATH_CMD_REGISTER_PATH;
    unsigned int replyfd = gettid() + 3;
    size_t path_len = strlen(pathname)+1;
    int fd = -1;
    char buf[INT_SIZE + INT_SIZE + INT_SIZE + PATH_MAX];
    int pos = 0;

    path_write_data(buf, &cmd, INT_SIZE, pos);
    path_write_data(buf, &replyfd, INT_SIZE, pos);
    path_write_data(buf, &path_len, SIZE_T_SIZE, pos);
    path_write_data(buf, pathname, path_len, pos);

    write(PATHSERVER_FD, buf, pos);
    read(replyfd, &fd, INT_SIZE);

    return fd;
}

int path_register_fs(const char *type)
{
    int cmd = PATH_CMD_REGISTER_FS;
    unsigned int replyfd = gettid() + 3;
    size_t type_len = strlen(type)+1;
    int fd = -1;
    char buf[INT_SIZE + INT_SIZE + SIZE_T_SIZE + PATH_MAX];
    int pos = 0;

    path_write_data(buf, &cmd, INT_SIZE, pos);
    path_write_data(buf, &replyfd, INT_SIZE, pos);
    path_write_data(buf, &type_len, SIZE_T_SIZE, pos);
    path_write_data(buf, type, type_len, pos);

    write(PATHSERVER_FD, buf, pos);
    read(replyfd, &fd, INT_SIZE);

    return fd;
}

int mount(const char *src, const char *dst, const char *type, int flags)
{
    int cmd = PATH_CMD_MOUNT;
    unsigned int replyfd = gettid() + 3;
    size_t slen = strlen(src)+1;
    size_t dlen = strlen(dst) + 1;
    size_t tlen = strlen(type) + 1;
    int status;
    char buf[INT_SIZE +
             INT_SIZE +
             SIZE_T_SIZE +
             PATH_MAX +
             SIZE_T_SIZE +
             PATH_MAX +
             SIZE_T_SIZE +
             FS_TYPE_MAX];
    int pos = 0;

    path_write_data(buf, &cmd, INT_SIZE, pos);
    path_write_data(buf, &replyfd, INT_SIZE, pos);
    path_write_data(buf, &slen, SIZE_T_SIZE, pos);
    path_write_data(buf, src, slen, pos);
    path_write_data(buf, &dlen, SIZE_T_SIZE, pos);
    path_write_data(buf, dst, dlen, pos);
    path_write_data(buf, &tlen, SIZE_T_SIZE, pos);
    path_write_data(buf, type, tlen, pos);

    write(PATHSERVER_FD, buf, pos);
    read(replyfd, &status, INT_SIZE);

    return status;
}

