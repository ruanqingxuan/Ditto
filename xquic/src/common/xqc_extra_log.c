#include "src/common/xqc_extra_log.h"
#include "src/common/xqc_log.h"

const char *
xqc_extra_log_type_str(xqc_extra_log_type_t type)
{
    static const char *event_type2str[] = {
            [XQC_CS_TEST]               = "XQC_CS_TEST",
            [XQC_AR_TEST]               = "XQC_AR_TEST"
    };
    return event_type2str[type];
}

void xqc_extra_log_implement(xqc_log_t *ori_log, xqc_extra_log_t *log, const char* func, const char *fmt, ...) {
  int fd = log->log_fd;

  xqc_extra_log_type_t type = log->log_type;
  unsigned char   buf[1024] = {0};
  unsigned char  *p = buf;
  unsigned char  *last = buf + sizeof(buf);

  // time
  char time[64];
  xqc_log_time(time, sizeof(time));
  p = xqc_sprintf(p, last, "[%s] ", time);

  // log type
  p = xqc_sprintf(p, last, "[%s] ", xqc_extra_log_type_str(type));

  // original log
  if (ori_log->scid != NULL) {
        p = xqc_sprintf(p, last, "[scid:%s] [%s] ", ori_log->scid, func);
  } else {
      p = xqc_sprintf(p, last, "[%s] ", func);
  }

  // log
  va_list args;
  va_start(args, fmt);
  p = xqc_vsprintf(p, last, fmt, args);
  va_end(args);

  // \n
  p = xqc_sprintf(p, last, "\n");

  if (p + 1 < last) {
      /* may use printf("%s") outside, add '\0' and don't count into size */
      *p = '\0';
  }

  // print
  write(fd, buf, p - buf);
}