/*
 * Id:             $Id: logging.cc 5410 2015-04-15 04:22:29Z cseagle $
 * Last Updated:   $LastChangedDate: 2015-04-15 04:22:29 +0000 (Wed, 15 Apr 2015) $ 
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <stdarg.h>

#include "logging.h"

static unsigned int msg_no = 0;

void log_ok(const char *msg, ...) {
   char *fmt;
   if (asprintf(&fmt, "ok %u - %s", ++msg_no, msg) != -1) {
      va_list ap;
      va_start(ap, msg);
      vfprintf(stderr, fmt, ap);
      va_end(ap);
      free(fmt);
   }
   else {
      fprintf(stderr, "not ok %u - asprintf failure in log_ok.\n", ++msg_no);
   }
}

void log_fail(const char *msg, ...) {
   char *fmt;
   if (asprintf(&fmt, "not ok %u - %s", ++msg_no, msg) != -1) {
      va_list ap;
      va_start(ap, msg);
      vfprintf(stderr, fmt, ap);
      va_end(ap);
      free(fmt);
   }
   else {
      fprintf(stderr, "not ok %u - asprintf failure in log_fail.\n", ++msg_no);
   }
}

void log_error(const char *prefix) {
   char errmsg[1024];
   errmsg[0] = 0;
#if (_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) && ! _GNU_SOURCE
   strerror_r(errno, errmsg, sizeof(errmsg));
   log_fail("%s: %s\n", prefix, errmsg);
#else
   char *res = strerror_r(errno, errmsg, sizeof(errmsg));
   log_fail("%s: %s\n", prefix, res);
#endif
}

void log(const char *msg, ...) {
#ifdef DEBUG
   char *fmt;
   if (asprintf(&fmt, "# %s", msg) != -1) {
      va_list ap;
      va_start(ap, msg);
      vfprintf(stderr, fmt, ap);
      va_end(ap);
      free(fmt);
   }
   else {
      fprintf(stderr, "not ok %u - asprintf failure in log.\n", ++msg_no);
   }
#endif
}

