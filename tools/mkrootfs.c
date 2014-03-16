#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <error.h>
#include <errno.h>
#include "file_metadata.h"

#define ERR_NO_DIR 1
#define ERR_NO_OUT 2
#define ERR_OPEN_OUT 3
#define ERR_OPEN_FILE 4

#define ERR(err) (ERR_##err)

#define PATH_LEN 31
#define BUF_SIZE 1024

size_t fwrite_off(const void *ptr, size_t size, size_t nmemb, FILE *stream, off_t off)
{
    fseek(stream, off, SEEK_SET);
    return fwrite(ptr, size, nmemb, stream);
}

int procfile(const char *filename, char *fullpath, FILE *outfile)
{
    FILE *infile;
    char buf[BUF_SIZE];
    size_t size;

    infile = fopen(fullpath, "rb");
    if (!infile) {
        error(ERR(OPEN_FILE), errno, "Cannot open file '%s'\n", fullpath);
    }

    while ((size = fread(buf, 1, BUF_SIZE, infile))) {
        fwrite(buf, 1, size, outfile);
    }

    fclose(infile);

    return ftell(outfile);
}

int procdir(const char *dirname, char *fullpath, FILE *outfile)
{
    DIR *dirfile;
    struct dirent *direntry;

    strcat(fullpath, "/");
    size_t fullpath_len = strlen(fullpath);

    dirfile = opendir(fullpath);
    if (!dirfile) {
        error(ERR(OPEN_OUT), errno, "Cannot open directory '%s'\n", fullpath);
    }

    int parent_metadata_offset = ftell(outfile) - sizeof(struct file_metadata_t);
    int prev_metadata_offset = 0; /* prev = 0 means no prev */
    int next_metadata_offset = ftell(outfile);
    int this_metadata_offset = next_metadata_offset;

    struct file_metadata_t file_metadata;
    file_metadata.parent = parent_metadata_offset;
    file_metadata.prev = 0;
    file_metadata.next = 0;

    /* Scan entries */
    while ((direntry = readdir(dirfile))) {
        /* Ignore hidden files and itself */
        if (*direntry->d_name == '.')
            continue;

        this_metadata_offset = next_metadata_offset;

        file_metadata.prev = prev_metadata_offset;
        strncpy((void*)file_metadata.name, direntry->d_name, PATH_LEN);

        strcpy(fullpath + fullpath_len, direntry->d_name);

        /* Reservion for this file_metadata */
        fwrite_off(&file_metadata, sizeof(file_metadata), 1, outfile, this_metadata_offset);

        /* Process file_metadata */
        if (direntry->d_type == DT_DIR) {
            file_metadata.isdir = 1;
            next_metadata_offset = procdir(direntry->d_name, fullpath, outfile);
        }
        else {
            file_metadata.isdir = 0;
            next_metadata_offset = procfile(direntry->d_name, fullpath, outfile);
        }

        file_metadata.next = next_metadata_offset;
        file_metadata.len = next_metadata_offset - (this_metadata_offset + sizeof(file_metadata));

        /* Write file_metadata */
        fwrite_off(&file_metadata, sizeof(file_metadata), 1, outfile, this_metadata_offset);

        prev_metadata_offset = this_metadata_offset;
    }

    /* Clear next of last file_metadata */
    if (file_metadata.next != 0) {
        file_metadata.next = 0;
        fwrite_off(&file_metadata, sizeof(file_metadata), 1, outfile, this_metadata_offset);
    }


    closedir(dirfile);

    return next_metadata_offset;
}

int main (int argc, char *argv[])
{
    int c;
    FILE *outfile;
    const char *outname = NULL;
    const char *dirname = NULL;
    char fullpath[PATH_MAX] = {0};

    while ((c = getopt(argc, argv, "o:d:")) != -1) {
        switch (c) {
            case 'd':
                if (optarg)
                    dirname = optarg;
                else
                    error(ERR(NO_DIR), EINVAL,
                          "-d option need a input directory.\n");
                break;
            case 'o':
                if (optarg)
                    outname = optarg;
                else
                    error(ERR(NO_OUT), EINVAL,
                          "-o option need a output file name.\n");
                break;
            default:
                ;
        }
    }

    if (!dirname) {
        dirname = ".";
    }

    if (outname)
        outfile = fopen(outname, "wb");
    else
        outfile = stdout;

    if (!outfile) {
        error(ERR(OPEN_OUT), errno,
              "Cannot open output file '%s'\n", outname);
    }

    strcpy(fullpath, dirname);

    /* Reservion for root file_metadata */
    struct file_metadata_t file_metadata;
    fwrite_off(&file_metadata, sizeof(file_metadata), 1, outfile, 0);
    size_t end = procdir(dirname, fullpath, outfile);

    file_metadata.parent = 0;
    file_metadata.prev = 0;
    file_metadata.next = 0;
    file_metadata.isdir = 1;
    file_metadata.len = end - sizeof(file_metadata);
    fwrite_off(&file_metadata, sizeof(file_metadata), 1, outfile, 0);

    /* Clean up*/
    fclose(outfile);

    return 0;
}
