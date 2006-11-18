#include "copyright.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include "defs.h"
#include "struct.h"
#include "data.h"

void blog_file(char *class, char *file)
{
    if (fork() == 0) {
        execl("blog", "blog", class, file, NULL);
        perror("blog");
        _exit(1);
    }
}

void blog_printf(char *class, const char *fmt, ...)
{
    va_list args;
    char name[MSG_LEN];
    struct timeval tv;
    FILE *file;

    gettimeofday(&tv, (struct timezone *) 0);
    sprintf(name, "%s.%d", class, (int) tv.tv_sec);
    file = fopen(name, "w");
    va_start(args, fmt);
    vfprintf(file, fmt, args);
    va_end(args);
    fclose(file);
    blog_file(class, name);
}

