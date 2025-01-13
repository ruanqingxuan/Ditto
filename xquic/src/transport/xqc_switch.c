#include "src/transport/xqc_switch.h"
#include "src/transport/xqc_ip_CCA_info.h"
#include "src/transport/xqc_stream_CCA_info.h"
#include "src/transport/xqc_extra_sample.h"
#include "src/transport/xqc_pacing.h"
#include "src/common/xqc_extra_log.h"
#include "src/common/xqc_memory_pool.h"


const char *cong_ctrl_str[] = {
  "copa",
  "bbr",
#ifndef XQC_DISABLE_RENO
  "reno",
#endif
#ifdef XQC_ENABLE_BBR2
  "bbr2",
#endif
   "cubic",
    // "unlimited"
};

xqc_cong_ctrl_callback_t *cong_ctrl_arr[] = {
  &xqc_copa_cb,
  &xqc_bbr_cb,
#ifndef XQC_DISABLE_RENO
  &xqc_reno_cb,
#endif
#ifdef XQC_ENABLE_BBR2
  &xqc_bbr2_cb,
#endif
  &xqc_cubic_cb,
    // &xqc_unlimited_cc_cb,
};

uint64_t CCA_time_arr[XQC_CCA_NUM] = {0};

int xqc_switch_ctx_init(xqc_send_ctl_t *send_ctl)
{
  xqc_cong_ctrl_callback_t *cong_ctrl = send_ctl->ctl_cong_callback;
  xqc_switch_ctx_t *switch_ctx = send_ctl->ctl_switch_ctx;
  xqc_switch_pq_t *switch_pq = NULL;
  xqc_connection_t *conn = send_ctl->ctl_conn;

  /* init switch ctx's priority queue */
  switch_pq = xqc_calloc(1, sizeof(xqc_switch_pq_t));

  if (!xqc_switch_pq_init_default(switch_pq)) {
    xqc_log(send_ctl->ctl_conn->log, XQC_LOG_INFO, "|CCA switching|switch_ctx create|");
  } else {
    return XQC_ERROR;
  }
  switch_ctx->ctx_CCA_queue = switch_pq;
  switch_ctx->ctx_cgnum = 0;

  /* init switch ctx by CCA_info */
  xqc_ip_CCA_info_t *CCA_infos = xqc_ip_get_CCA_info(send_ctl);
  if (CCA_infos == NULL) {
    xqc_free(switch_pq);
    return XQC_ERROR;
  }

  if (send_ctl->ctl_conn->conn_settings.enable_CCA_switching) {
    for (int i = 0; i < XQC_CCA_NUM; i++) {
      /* init power */
      float metric = 0.0;
      if (send_ctl->ctl_metric_cb) {
        metric = send_ctl->ctl_metric_cb(&send_ctl->CCA_sampler, CCA_infos, i);
      } else {
        xqc_log(conn->log, XQC_LOG_INFO, "|CCA switching|metric_cb is not init|");
      }

      /* start polling */
      metric = 0.1 * metric + 0.9;
      xqc_switch_pq_push(switch_pq, metric, (xqc_switch_state_t)i);
      xqc_log(conn->log, XQC_LOG_DEBUG, "|CCA switching|init|push %s|metric %.6f|", cong_ctrl_str[i], metric);

      /* init cong stack */
      send_ctl->ctl_cong_stack[i] = xqc_pcalloc(conn->conn_pool, cong_ctrl_arr[i]->xqc_cong_ctl_size());
      if (cong_ctrl_arr[i]->xqc_cong_ctl_init_bbr) {
        cong_ctrl_arr[i]->xqc_cong_ctl_init_bbr(send_ctl->ctl_cong_stack[i],
                                                &send_ctl->sampler, conn->conn_settings.cc_params);
      } else {
        cong_ctrl_arr[i]->xqc_cong_ctl_init(send_ctl->ctl_cong_stack[i], send_ctl, conn->conn_settings.cc_params);
      }
    }
  } else {
    xqc_switch_pq_push(switch_pq, 0, send_ctl->ctl_conn->conn_settings.init_cong_ctrl);
  }

  xqc_switch_pq_elem_t *top_elem = xqc_switch_pq_top(switch_pq);
  if (send_ctl->ctl_conn->conn_settings.enable_CCA_switching) {
    send_ctl->ctl_cong_callback = cong_ctrl_arr[top_elem->switch_state];
    send_ctl->ctl_cong = send_ctl->ctl_cong_stack[top_elem->switch_state];
  }
  switch_ctx->ctx_state = top_elem->switch_state;
  switch_ctx->ctx_future_state = XQC_CCA_NUM;
  switch_ctx->ctx_last_use_time = xqc_monotonic_timestamp();

  xqc_extra_log(send_ctl->ctl_conn->log, send_ctl->ctl_conn->CS_extra_log, "[init CCA:%s] [metric:%.6f]", cong_ctrl_str[top_elem->switch_state], top_elem->metric);
  xqc_log(send_ctl->ctl_conn->log, XQC_LOG_INFO, "|CCA switching|init CCA %s|\n", cong_ctrl_str[top_elem->switch_state]);
  
  xqc_switch_pq_pop(switch_pq);
  return XQC_OK;
}

void xqc_switch_ctx_destroy(xqc_send_ctl_t *send_ctl)
{
  xqc_switch_ctx_t *switch_ctx = send_ctl->ctl_switch_ctx;
  xqc_switch_pq_t *switch_pq = switch_ctx->ctx_CCA_queue;
  xqc_ip_CCA_info_t *CCA_infos = xqc_ip_get_CCA_info(send_ctl);

  /* save CCA_info of current using CCA */
  int CCA_id = switch_ctx->ctx_state;
  CCA_infos[CCA_id].CCA_cnt = CCA_time_arr[CCA_id];
  if (send_ctl->ctl_conn->conn_settings.enable_CCA_switching) {
    for (int i = 0; i < XQC_CCA_NUM - 1; i++) {
      xqc_switch_pq_elem_t *elem = xqc_switch_pq_element(switch_pq, i);
      CCA_id = elem->switch_state;
      CCA_infos[CCA_id].CCA_cnt = CCA_time_arr[CCA_id];
    }
  }
  
  for (size_t i = 0; i < XQC_CCA_NUM; i++) {
    xqc_extra_log(send_ctl->ctl_conn->log, send_ctl->ctl_conn->CS_extra_log, "[CCA:%s] [used cnt:%d]", cong_ctrl_str[i], CCA_infos[i].CCA_cnt);
    xqc_log(send_ctl->ctl_conn->log, XQC_LOG_INFO, "|CCA switching|save CCA info|CCA id:%d|used cnt:%d|", i, CCA_infos[i].CCA_cnt);
  }
  
  xqc_switch_pq_destroy(switch_ctx->ctx_CCA_queue);
  xqc_free(switch_ctx->ctx_CCA_queue);
}

void xqc_switch_CCA_implement(xqc_send_ctl_t *send_ctl)
{
  xqc_switch_ctx_t *switch_ctx = send_ctl->ctl_switch_ctx;
  xqc_switch_pq_t *switch_pq = switch_ctx->ctx_CCA_queue;
  xqc_connection_t *conn = send_ctl->ctl_conn;
  /* compare the current power and the max power in switch_pq */
  float cur_metric = send_ctl->ctl_switch_ctx->ctx_metric;
  float max_metric = 0.0;
  
  xqc_switch_pq_elem_t *top_elem = xqc_switch_pq_top(switch_pq);
  if (top_elem) {
    max_metric = top_elem->metric;
  } else {
    xqc_log(send_ctl->ctl_conn->log, XQC_LOG_ERROR, "|CCA switching|no CCA in switch_pq|");
  }

  /* if max_power > cur_power, switch the CCA */
  if (cur_metric < max_metric && top_elem) {
    xqc_switch_state_t future_state = top_elem->switch_state;
    if (switch_ctx->ctx_cgnum > XQC_SWITCH_THRES) {
      switch_ctx->ctx_cgnum = 0;
      /* maintain the switch_pq */
      xqc_switch_state_t switch_state = top_elem->switch_state;

      /* update time */ 
      CCA_time_arr[switch_ctx->ctx_state] += (xqc_monotonic_timestamp() - switch_ctx->ctx_last_use_time);
      switch_ctx->ctx_last_use_time = xqc_monotonic_timestamp();

      /* switch */
      send_ctl->ctl_cong_callback = cong_ctrl_arr[future_state];
      send_ctl->ctl_cong = send_ctl->ctl_cong_stack[future_state];
      xqc_extra_log(send_ctl->ctl_conn->log, send_ctl->ctl_conn->CS_extra_log, "[origin_CCA:%s] [origin_metric:%.6f] [current_CCA:%s] [current_metric:%.6f]"
          , cong_ctrl_str[switch_ctx->ctx_state], cur_metric, cong_ctrl_str[switch_state], max_metric);
      xqc_log(send_ctl->ctl_conn->log, XQC_LOG_INFO, "|CCA switching|switch CCA from %s to %s|", cong_ctrl_str[switch_ctx->ctx_state], cong_ctrl_str[switch_state]);

      /* update switch pq */ 
      xqc_switch_pq_pop(switch_pq);
      xqc_switch_pq_push(switch_pq, cur_metric, switch_ctx->ctx_state);
      switch_ctx->ctx_state = future_state;
      switch_ctx->ctx_future_state = XQC_CCA_NUM;
    } else {
      if (switch_ctx->ctx_future_state == XQC_CCA_NUM) {
        switch_ctx->ctx_future_state = future_state;
        uint64_t pacing_rate, cwnd;
        xqc_cong_ctrl_callback_t *cong_cb = send_ctl->ctl_cong_callback;
        void *cong = send_ctl->ctl_cong;
        if (cong_cb->xqc_cong_ctl_get_cwnd) {
          pacing_rate = xqc_pacing_rate_calc(&send_ctl->ctl_pacing);
          cwnd = cong_cb->xqc_cong_ctl_get_cwnd(cong);
        }
        if (cong_cb->xqc_cong_ctl_get_pacing_rate) {
          pacing_rate = cong_cb->xqc_cong_ctl_get_pacing_rate(cong);
        }
        cong_cb = cong_ctrl_arr[future_state];
        cong = send_ctl->ctl_cong_stack[future_state];
        if (cong_cb->xqc_cong_ctl_set_cwnd) {
          cong_cb->xqc_cong_ctl_set_cwnd(cong, cwnd);
        }
        if (cong_cb->xqc_cong_ctl_set_pacing_rate) {
          cong_cb->xqc_cong_ctl_set_pacing_rate(cong, pacing_rate);
        }
      }
      
      xqc_log(send_ctl->ctl_conn->log, XQC_LOG_INFO, "|CCA switching|CCA:%s|current cgnum:%d|", cong_ctrl_str[switch_ctx->ctx_state], switch_ctx->ctx_cgnum);
      xqc_extra_log(send_ctl->ctl_conn->log, send_ctl->ctl_conn->CS_extra_log, "[CCA:%s] [current cgnum:%d]", cong_ctrl_str[switch_ctx->ctx_state], switch_ctx->ctx_cgnum);
      switch_ctx->ctx_cgnum++;
    }
  }
}

void xqc_switch_CCA(xqc_send_ctl_t *send_ctl) 
{
  /* update samples */
  xqc_update_CCA_info_sample(&send_ctl->CCA_sampler, send_ctl);

  xqc_ip_CCA_info_t *CCA_info = xqc_ip_get_CCA_info(send_ctl);
  int i = send_ctl->ctl_switch_ctx->ctx_state;
  if (send_ctl->ctl_metric_cb) {
      send_ctl->ctl_switch_ctx->ctx_metric = send_ctl->ctl_metric_cb(&send_ctl->CCA_sampler, CCA_info, i);
      xqc_log(send_ctl->ctl_conn->log, XQC_LOG_INFO, "|CCA switching|CCA:%s|metric:%.6f|", cong_ctrl_str[i], send_ctl->ctl_switch_ctx->ctx_metric);
      xqc_extra_log(send_ctl->ctl_conn->log, send_ctl->ctl_conn->CS_extra_log, "[CCA:%s] [metric:%.6f]", cong_ctrl_str[i], send_ctl->ctl_switch_ctx->ctx_metric);
  } else {
      xqc_log(send_ctl->ctl_conn->log, XQC_LOG_INFO, "|CCA switching|metric_cb is not init|");
  }
  if (send_ctl->ctl_conn->conn_settings.enable_CCA_switching) {
    xqc_switch_CCA_implement(send_ctl);
  }
}

xqc_cong_ctrl_callback_t *xqc_switch_get_future_cc_cb(xqc_send_ctl_t *send_ctl) {
  xqc_switch_state_t future_state = send_ctl->ctl_switch_ctx->ctx_future_state;
  xqc_cong_ctrl_callback_t *cong_ctrl = NULL;
  if (future_state != XQC_CCA_NUM) {
    cong_ctrl = cong_ctrl_arr[future_state];
  }
  return cong_ctrl;
}