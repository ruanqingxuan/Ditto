#ifndef _XQC_H_EXTRA_LOG_INCLUDED_
#define _XQC_H_EXTRA_LOG_INCLUDED_

#include <time.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <xquic/xquic.h>
#include "src/common/xqc_config.h"
#include "src/common/xqc_malloc.h"
#include "src/common/xqc_str.h"
#include "src/common/xqc_time.h"
#include "src/common/xqc_log.h"

typedef enum {
  XQC_CS_TEST,
  XQC_AR_TEST
} xqc_extra_log_type_t;

typedef struct xqc_extra_log_s {
  int log_fd;
  xqc_extra_log_type_t log_type;
} xqc_extra_log_t;

static inline xqc_extra_log_t * 
xqc_extra_log_init(const char* file, xqc_extra_log_type_t type) {
  xqc_extra_log_t* log = (xqc_extra_log_t*)xqc_malloc(sizeof(xqc_extra_log_t));
  if (log == NULL) {
      return NULL;
  }

  log->log_fd = open(file, O_RDWR | O_CREAT | O_APPEND, 0666);
  log->log_type = type;
  return log;
}

static inline void 
xqc_extra_log_release(xqc_extra_log_t *log) {
  close(log->log_fd);
  xqc_free(log);
  log = NULL;
}

void xqc_extra_log_implement(xqc_log_t *ori_log, xqc_extra_log_t *log, const char* func, const char *fmt, ...);

#define xqc_extra_log(ori_log, log, ...) \
    do { \
            xqc_extra_log_implement(ori_log, log, __FUNCTION__, __VA_ARGS__); \
    } while (0)


#endif