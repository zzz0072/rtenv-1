#ifndef UTIL_H_20130921
#define UTIL_H_20130921
#include <stddef.h>

#define MAX_MSG_CHARS (32)

void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *dest, int c, size_t n);
void  puts(const char *s);
void  print(char *msg);
int printf(const char *format, ...);
int sprintf(char *str, const char *format, ...);

size_t strlen(const char *s);
int   strcmp(const char *a, const char *b);
int   strncmp(const char *a, const char *b, size_t n);
char *strcat(char *dest, const char *src);
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t n);


/* Warning!! Last atoi buf element needs to be set to \0 */
/*           for itoa(), htoa() and addrtoa()            */
/* Example: itoa_buf[MAX_ITOA_CHARS - 1] = 0;            */
/*          itoa_buf = itoa(100, itoa_buf)               */
enum int_type_t {
    SIGNED_INT,
    UNSIGNED_INT
};
#define MAX_ITOA_CHARS (32)

#define itoa(val, str) num_to_string(val, 10, str, SIGNED_INT)
#define htoa(val, str) num_to_string(val, 16, str, SIGNED_INT)
char* num_to_string(unsigned int val, int base, char *buf, enum int_type_t int_type);

#endif /* UTIL_H_20130921 */
