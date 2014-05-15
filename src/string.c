#include "string.h"

#include "stm32f10x.h"
#include <stdarg.h> /* For variable list */
#include "syscall.h"
#include "stm32f10x.h"

/* TODO: Modulize kernel.c */
extern int mq_open(const char *name, int oflag);

/* Hand made tools */
int strncmp(const char *str_a, const char *str_b, size_t n)
{
    /* Find not matched char or reach n */
    while(*str_a == *str_b && *str_a && n > 1) {
        str_a++; str_b++; n--;
    }

    return (int) (*str_a - *str_b);
}

char *strcat(char *dest, const char *src)
{
    size_t src_len  = strlen(src);
    size_t dest_len = strlen(dest);

    if (!dest || !src) {
        return dest;
    }

    memcpy(dest + dest_len, src, src_len + 1);
    return dest;
}

char* num_to_string(unsigned int val, int base, char *buf, enum int_type_t int_type)
{
    char has_minus = 0;
    int i = 30;

    /* Sepecial case: 0 */
    if (val == 0) {
        buf[1] = '0';
        return &buf[1];
    }

    if (int_type == SIGNED_INT && (int)val < 0) {
        val = (int)-val;
        has_minus = 1;
    }

    for (; val && (i - 1) ; --i, val /= base)
        buf[i] = "0123456789abcdef"[val % base];

    if (has_minus) {
        buf[i] = '-';
        return &buf[i];
    }
    return &buf[i + 1];
}

void puts(const char *msg)
{
    int fdout = mq_open("/tmp/mqueue/out", 0);
    size_t rval = 0;

    if (!msg) {
        return;
    }

    rval = write(fdout, msg, strlen(msg) + 1);
    if (rval == -1) {
        return;
    }
}

static int printf_cb(char *dest, const char *src)
{
    puts(src);

    return 0; /* just for obeying prototype */
}

static int sprintf_cb(char *dest, const char *src)
{
    return (int)strcat(dest, src);
}

typedef int (*proc_str_func_t)(char *, const char *);

/* Common body for sprintf and printf */
static int base_printf(proc_str_func_t proc_str, \
                char *dest, const char *fmt_str, va_list param)
{
    char  param_chr[] = {0, 0};
    int   param_int = 0;

    char *str_to_output = 0;
    char itoa_buf[MAX_ITOA_CHARS] = {0};
    int  curr_char  = 0;

    /* Make sure strlen(dest) is 0
     * for first strcat */
    if (dest) {
        dest[0] = 0;
    }

    /* Let's parse */
    while (fmt_str[curr_char]) {
        /* Deal with normal string
         * increase index by 1 here  */
        if (fmt_str[curr_char++] != '%') {
            param_chr[0]  = fmt_str[curr_char - 1];
            str_to_output = param_chr;
        }
        /* % case-> retrive latter params */
        else {
            switch (fmt_str[curr_char]) {
                case 'S':
                case 's':
                    {
                        str_to_output = va_arg(param, char *);
                    }
                    break;

                case 'd':
                case 'D':
                    {
                       param_int     = va_arg(param, int);
                       str_to_output = itoa(param_int, itoa_buf);
                    }
                    break;

                case 'X':
                case 'x':
                    {
                       param_int     = va_arg(param, int);
                       str_to_output = htoa(param_int, itoa_buf);
                    }
                    break;

                case 'c':
                case 'C':
                    {
                        param_chr[0]  = (char) va_arg(param, int);
                        str_to_output = param_chr;
                        break;
                    }

                default:
                    {
                        param_chr[0]  = fmt_str[curr_char];
                        str_to_output = param_chr;
                    }
            } /* switch (fmt_str[curr_char])      */
            curr_char++;
        }     /* if (fmt_str[curr_char++] == '%') */
        proc_str(dest, str_to_output);
    }         /* while (fmt_str[curr_char])       */

    return curr_char;
}

int sprintf(char *str, const char *format, ...)
{
    int rval = 0;
    va_list param = {0};

    va_start(param, format);
    rval = base_printf(sprintf_cb, (char *)str, format, param);
    va_end(param);

    return rval;
}

int printf(const char *format, ...)
{
    int rval = 0;
    va_list param = {0};

    va_start(param, format);
    rval = base_printf(printf_cb, (char *)0, format, param);
    va_end(param);

    return rval;
}

#ifndef USE_ASM_OPTI_FUNC
int strcmp(const char *s1, const char *s2)
{
    while (*s1 && *s1 == *s2) {
          s1++, s2++;
    }
    return (*s1 - *s2);
}

size_t strlen(const char *string)
{
    int chars = 0;

    while(*string++) {
        chars++;
    }
    return chars;
}

void *memcpy(void *dest, const void *src, size_t n)
{
    int i;

    for (i = 0; i < n; i++) {
        *((char *)dest + i) = *((char *)src + i);
    }
    return dest;
}

void *memset(void *dest, int c, size_t n)
{
    int i;

    for (i = 0; i < n; i++) {
        *((char *)dest + i) = c;
    }
    return dest;
}
#else /* ASM optimized version */
int strcmp(const char *a, const char *b) __attribute__ ((naked));
int strcmp(const char *a, const char *b)
{
    __asm__(
        "strcmp_lop:                \n"
        "   ldrb    r2, [r0],#1     \n"
        "   ldrb    r3, [r1],#1     \n"
        "   cmp     r2, #1          \n"
        "   it      hi              \n"
        "   cmphi   r2, r3          \n"
        "   beq     strcmp_lop      \n"
        "    sub     r0, r2, r3      \n"
        "   bx      lr              \n"
        :::
    );
}

size_t strlen(const char *s) __attribute__ ((naked));
size_t strlen(const char *s)
{
    __asm__(
        "    sub  r3, r0, #1            \n"
        "strlen_loop:               \n"
        "    ldrb r2, [r3, #1]!        \n"
        "    cmp  r2, #0                \n"
        "   bne  strlen_loop        \n"
        "    sub  r0, r3, r0            \n"
        "    bx   lr                    \n"
        :::
    );
}
#endif /* USE_ASM_OPTI_FUNC */
