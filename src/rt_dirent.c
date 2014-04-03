#include "file.h"
#include "path.h"
#include "syscall.h"
#include "rt_string.h"
#include "rt_dirent.h"

RT_DIR opendir(const char *pathname)
{
    int cmd = PATH_CMD_OPENDIR;
    unsigned int replyfd = gettid() + 3;
    size_t path_len = strlen(pathname) + 1;
    char buf[INT_SIZE + INT_SIZE + SIZE_T_SIZE + PATH_MAX];
    int pos = 0;
    RT_DIR rval = 0;

    /* only take absolutely path */
    if (pathname == 0 || pathname[0] != '/') {
        return -1;
    }

    /* Send request to path server */
    path_write_data(buf, &cmd, INT_SIZE, pos);
    path_write_data(buf, &replyfd, INT_SIZE, pos);
    path_write_data(buf, &path_len, SIZE_T_SIZE, pos);
    path_write_data(buf, pathname, path_len, pos);

    write(PATHSERVER_FD, buf, pos);

    /* Response */
    read(replyfd, &rval, sizeof(RT_DIR));

    return rval;
}

int closedir(RT_DIR dir)
{
    int cmd = PATH_CMD_CLOSEDIR;
    unsigned int replyfd = gettid() + 3;
    char buf[INT_SIZE + INT_SIZE + SIZE_T_SIZE + PATH_MAX];
    int pos = 0;
    RT_DIR rval = 0;

    /* Send request to path server */
    path_write_data(buf, &cmd, INT_SIZE, pos);
    path_write_data(buf, &replyfd, INT_SIZE, pos);
    path_write_data(buf, &dir, sizeof(RT_DIR), pos);

    write(PATHSERVER_FD, buf, pos);

    /* Response */
    read(replyfd, &rval, INT_SIZE);

    return rval;
}

