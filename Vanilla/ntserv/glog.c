#include "copyright.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "defs.h"
#include "struct.h"
#include "data.h"

static FILE *glog = NULL;

void glog_close(void) {
  if (glog != NULL) {
    fclose(glog);
    glog = NULL;
  }
}

int glog_open() {
  if (glog != NULL) return 0;
  glog = fopen(GodLog, "a");
  if (glog != NULL) {
    atexit(glog_close);
    return 0;
  }
  perror(GodLog);
  return -1;
}

void glog_printf(const char *fmt, ...) {
  va_list args;

  va_start(args, fmt);
  vfprintf(glog, fmt, args);
  va_end(args);
}

void glog_flush() {
  fflush(glog);
}
