#include "src/congestion_control/xqc_coward.h"
#include "src/common/xqc_config.h"
#include "src/common/xqc_extra_log.h"
#include <math.h>

#define XQC_COWARD_MSS              (XQC_MSS)
#define XQC_COWARD_MIN_WIN          (4 * XQC_COWARD_MSS)
#define XQC_COWARD_INIT_WIN         (32 * XQC_COWARD_MSS)

size_t
xqc_coward_size()
{
    return sizeof(xqc_coward_t);
}

static void
xqc_coward_init(void *cong_ctl, xqc_send_ctl_t *ctl_ctx, xqc_cc_params_t cc_params)
{
    xqc_coward_t *coward = (xqc_coward_t*)cong_ctl;
    coward->min_cwnd = XQC_COWARD_MIN_WIN;
    coward->cwnd = XQC_COWARD_INIT_WIN;
    coward->send_ctl = ctl_ctx;
}

static void
xqc_coward_restart_from_idle(void *cong_ctl, uint64_t arg)
{
    return;
}

static int
xqc_coward_in_recovery(void *cong_ctl)
{
    return 0;
}

static uint64_t
xqc_coward_get_cwnd(void *cong_ctl)
{
    xqc_coward_t *coward = (xqc_coward_t *)cong_ctl;
    xqc_connection_t *conn = coward->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    return coward->cwnd;
}

static void
xqc_coward_set_cwnd_CS(void *cong, uint64_t cwnd)
{
    xqc_coward_t *coward = (xqc_coward_t *)(cong);
    coward->cwnd = cwnd;
}

static void
xqc_coward_print_status(void *cong, xqc_sample_t *sampler)
{
    xqc_coward_t *coward = (xqc_coward_t *)(cong);
    xqc_send_ctl_t *send_ctl = sampler->send_ctl;
    xqc_connection_t *conn = send_ctl->ctl_conn;

    xqc_extra_log(conn->log, conn->CS_extra_log, "[cwnd:%d]", coward->cwnd);
}

static void
xqc_coward_on_switching(void *cong, xqc_bool_t is_better) {
    xqc_coward_t *coward = (xqc_coward_t *)(cong);
    xqc_send_ctl_t *send_ctl = coward->send_ctl;
    xqc_connection_t *conn = send_ctl->ctl_conn;
    xqc_extra_log(conn->log, conn->CS_extra_log, "[is_better:%d]", is_better);
    uint64_t new_cwnd = is_better ? coward->cwnd : coward->cwnd / 2;
    new_cwnd = xqc_max(new_cwnd, coward->min_cwnd);
    coward->cwnd = new_cwnd;
}

static int
xqc_coward_in_slow_start(void *cong_ctl) {
    return 1;
}

xqc_cong_ctrl_callback_t xqc_coward_cb = {
    .xqc_cong_ctl_size              = xqc_coward_size,
    .xqc_cong_ctl_init              = xqc_coward_init,
    .xqc_cong_ctl_get_cwnd          = xqc_coward_get_cwnd,
    .xqc_cong_ctl_restart_from_idle = xqc_coward_restart_from_idle,
    .xqc_cong_ctl_in_recovery       = xqc_coward_in_recovery,
    .xqc_cong_ctl_set_cwnd          = xqc_coward_set_cwnd_CS,
    .xqc_cong_ctl_print_status      = xqc_coward_print_status,
    .xqc_cong_ctl_on_switching      = xqc_coward_on_switching,
    .xqc_cong_ctl_in_slow_start     = xqc_coward_in_slow_start,
};