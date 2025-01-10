// added by jndu for CCA switching
#ifndef _XQC_SWITCH_H_INCLUDED
#define _XQC_SWITCH_H_INCLUDED

#include <xquic/xquic.h>
#include <xquic/xquic_typedef.h>
#include "src/common/xqc_malloc.h"
#include "src/transport/xqc_send_ctl.h"
#include "src/transport/xqc_switch_pq.h"

#define XQC_SWITCH_THRES  50

typedef struct xqc_switch_ctx_s{
  uint64_t            ctx_last_use_time;
  int                 ctx_cgnum;  // only when cgnum > threshold, switching occurs
  xqc_switch_state_t  ctx_state;
  xqc_switch_state_t  ctx_future_state;
  float               ctx_metric;
  xqc_switch_pq_t     *ctx_CCA_queue;            
} xqc_switch_ctx_t;


/**
 * @brief initialize one send_ctl's switch ctx.
 * @return 0 for error, 1 for success
*/
int xqc_switch_ctx_init (xqc_send_ctl_t *send_ctl);

/**
 * @brief destroy one send_ctl's switch ctx.
*/
void xqc_switch_ctx_destroy (xqc_send_ctl_t *send_ctl);

/**
 * @brief inplement CCA switching in stream level.
*/
void xqc_switch_CCA_implement (xqc_send_ctl_t *send_ctl);

/**
 * @brief func include updating sample, inplement CCA switching
 */
void xqc_switch_CCA(xqc_send_ctl_t *send_ctl);

xqc_cong_ctrl_callback_t *xqc_switch_get_future_cc_cb(xqc_send_ctl_t *send_ctl);

#endif