// added by jndu for CCA switching
#ifndef _XQC_SWITCH_H_INCLUDED
#define _XQC_SWITCH_H_INCLUDED

#include <xquic/xquic.h>
#include <xquic/xquic_typedef.h>
#include "src/common/xqc_malloc.h"
#include "src/transport/xqc_send_ctl.h"
#include "src/transport/xqc_switch_pq.h"

#define XQC_SWITCH_THRES 1

#define CWND_CHANGE 500
#define METRIC_CHANGE 0
#define CCA_SAMPLE_CHANGE 1
#define METRIC_THRES 0.5

typedef enum xqc_switch_status_s
{
    XQC_CS_SLOW_START,
    XQC_CS_PROBE_CCA,
    XQC_CS_STABLE_RUNNING
} xqc_switch_status_t;

typedef enum xqc_CCA_status_s
{
    XQC_CCA_NEED_CHANGE = -1,
    XQC_CCA_WAIT_FOR_EVALUATE,
    XQC_CCA_CAN_STABLE_RUNNING,
} xqc_CCA_status_t;

typedef struct xqc_switch_ctx_s
{
    xqc_send_ctl_t *ctx_send_ctl;
    uint64_t ctx_last_cwnd;
    float ctx_cwnd_change;
    xqc_switch_status_t ctx_status;
    uint64_t ctx_last_use_time;
    int ctx_cgnum; // only when cgnum > threshold, switching occurs
    xqc_switch_CCA_t ctx_CCA;
    xqc_switch_CCA_t ctx_future_CCA;
    float ctx_metric;
    float ctx_metric_cwnd_up;
    float ctx_metric_cwnd_down;
    xqc_stream_CCA_info_sampler_t ctx_metric_cwnd_up_sampler;
    xqc_stream_CCA_info_sampler_t ctx_metric_cwnd_down_sampler;
    xqc_switch_pq_t *ctx_CCA_queue;
    int8_t ctx_cg_status_num;
    int8_t ctx_cg_status_sample_cnt;
    xqc_bool_t can_use_coward;
    xqc_bool_t is_weak;
    float max_metric;
} xqc_switch_ctx_t;

/**
 * @brief initialize one send_ctl's switch ctx.
 * @return 0 for error, 1 for success
 */
int xqc_switch_ctx_init(xqc_send_ctl_t *send_ctl);

/**
 * @brief destroy one send_ctl's switch ctx.
 */
void xqc_switch_ctx_destroy(xqc_send_ctl_t *send_ctl);

/**
 * @brief implement CCA switching in stream level.
 */
void xqc_switch_CCA_implement(xqc_send_ctl_t *send_ctl);

/**
 * @brief func include updating sample, implement CCA switching
 */
void xqc_switch_CCA(xqc_send_ctl_t *send_ctl);

xqc_cong_ctrl_callback_t *xqc_switch_get_future_cc_cb(xqc_send_ctl_t *send_ctl);

static inline xqc_bool_t xqc_CCA_can_be_anomaly(float metric, float metric_thres)
{
    return metric < metric_thres;
}

/**
 * @brief for finding decrease reason
 * @return 0 for no info decrease, 1 for XQC_CCA_INFO_TRACE_UP info decrease,
 *         2 for XQC_CCA_INFO_TRACE_DOWN info decrease, 3 for both decrease
 */
xqc_bool_t xqc_CCA_check_metric_decrease_reason(xqc_switch_ctx_t *switch_ctx, xqc_stream_CCA_info_t *stream_CCA_info, xqc_CCA_info_trace_type_t cwnd_trace,
                                                xqc_bool_t can_be_weak, xqc_bool_t can_be_aggressive);

xqc_CCA_status_t xqc_evaluate_CCA(xqc_send_ctl_t *send_ctl, xqc_switch_status_t switch_status);

void xqc_CCA_make_in_step(xqc_send_ctl_t *send_ctl, xqc_switch_CCA_t future);

#endif