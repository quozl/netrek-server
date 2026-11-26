#ifndef __INCLUDED_strutil_h__
#define __INCLUDED_strutil_h__

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/*
 * Bounded string copy returning strlen(src) and guaranteeing NUL termination
 * whenever dstsize > 0.
 */
static inline size_t netrek_strlcpy(char *dst, const char *src, size_t dstsize)
{
    size_t srclen;

    if (src == NULL)
        src = "";
    srclen = strlen(src);
    if (dst != NULL && dstsize > 0) {
        size_t copy = srclen < dstsize - 1 ? srclen : dstsize - 1;
        if (copy > 0)
            memcpy(dst, src, copy);
        dst[copy] = '\0';
    }
    return srclen;
}

/*
 * Safe formatted append into a fixed buffer tracked by *pos.
 * Returns 0 if the formatted output fit without truncation, -1 otherwise.
 */
static inline int netrek_appendf(char *dst, size_t dstsize, size_t *pos,
                                 const char *fmt, ...)
    __attribute__((format(printf, 4, 5)));

static inline int netrek_appendf(char *dst, size_t dstsize, size_t *pos,
                                 const char *fmt, ...)
{
    va_list ap;
    int n;

    if (dst == NULL || pos == NULL || fmt == NULL || dstsize == 0 ||
        *pos >= dstsize)
        return -1;

    va_start(ap, fmt);
    n = vsnprintf(dst + *pos, dstsize - *pos, fmt, ap);
    va_end(ap);

    if (n < 0) {
        dst[*pos] = '\0';
        return -1;
    }
    if ((size_t) n >= dstsize - *pos) {
        *pos = dstsize - 1;
        dst[*pos] = '\0';
        return -1;
    }
    *pos += (size_t) n;
    return 0;
}

#endif /* __INCLUDED_strutil_h__ */
