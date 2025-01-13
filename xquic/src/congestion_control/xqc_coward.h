#ifndef _XQC_COWARD_H_INCLUDED_
#define _XQC_COWARD_H_INCLUDED_

#include <xquic/xquic_typedef.h>
#include <xquic/xquic.h>
#include "src/transport/xqc_send_ctl.h"

typedef struct {
    uint32_t min_cwnd;
    uint32_t cwnd;
    xqc_send_ctl_t *send_ctl;
} xqc_coward_t;


extern xqc_cong_ctrl_callback_t xqc_coward_cb;

#endif