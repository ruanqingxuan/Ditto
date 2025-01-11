#include "src/transport/xqc_switch.h"
#include "src/transport/xqc_ip_CCA_info.h"
#include "src/transport/xqc_stream_CCA_info.h"
#include "src/transport/xqc_extra_sample.h"
#include "src/transport/xqc_pacing.h"
#include "src/common/xqc_extra_log.h"
#include "src/common/xqc_memory_pool.h"

const float METRIC_RECOVER = 0.005;

const char *cong_ctrl_str[] = {

  
  "cubic",
  #ifndef XQC_DISABLE_RENO
  "reno",
  #endif
  "copa",
  "bbr",
  #ifdef XQC_ENABLE_BBR2
  "bbr2",
  #endif
  "coward"
    // "unlimited"
};

xqc_cong_ctrl_callback_t *cong_ctrl_arr[] = {
  
  
  &xqc_cubic_cb,
#ifndef XQC_DISABLE_RENO
  &xqc_reno_cb,
  #endif
  &xqc_copa_cb,
  &xqc_bbr_cb,
  #ifdef XQC_ENABLE_BBR2
  &xqc_bbr2_cb,
#endif
  // &xqc_pcc_proteus_cb
  &xqc_coward_cb,

    // &xqc_unlimited_cc_cb,
};

uint64_t CCA_time_arr[XQC_CCA_NUM] = {0};

int xqc_switch_ctx_init(xqc_send_ctl_t *send_ctl)
{
  xqc_cong_ctrl_callback_t *cong_ctrl = send_ctl->ctl_cong_callback;
  xqc_switch_ctx_t *switch_ctx = send_ctl->ctl_switch_ctx;
  xqc_switch_pq_t *switch_pq = NULL;
  xqc_connection_t *conn = send_ctl->ctl_conn;
  xqc_usec_t now = xqc_monotonic_timestamp();

  /* init switch ctx's priority queue */
  switch_pq = xqc_calloc(1, sizeof(xqc_switch_pq_t));

  if (!xqc_switch_pq_init_default(switch_pq)) {
    xqc_log(send_ctl->ctl_conn->log, XQC_LOG_INFO, "|CCA switching|switch_ctx create|");
  } else {
    return XQC_ERROR;
  }
  switch_ctx->ctx_CCA_queue = switch_pq;
  switch_ctx->ctx_cgnum = 0;
  switch_ctx->ctx_send_ctl = send_ctl;
  switch_ctx->can_use_coward = send_ctl->ctl_conn->conn_settings.can_use_coward;
  switch_ctx->is_weak = 0;

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
      if (send_ctl->ctl_metric_ip_info_cb) {
        metric = send_ctl->ctl_metric_ip_info_cb(CCA_infos, i);
      }
      metric = send_ctl->ctl_metric_sum_up_cb(1, 1, metric);
      /* start polling */
      if (i != XQC_COWARD || switch_ctx->can_use_coward) {
        xqc_switch_pq_push(switch_pq, metric, (xqc_switch_CCA_t)i, now);
      } 

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
    xqc_switch_pq_push(switch_pq, 0, send_ctl->ctl_conn->conn_settings.init_cong_ctrl, now);
  }

  if (send_ctl->ctl_conn->conn_settings.enable_CCA_switching) {
    send_ctl->ctl_cong_callback = cong_ctrl_arr[XQC_COPA];
    send_ctl->ctl_cong = send_ctl->ctl_cong_stack[XQC_COPA];
    switch_ctx->ctx_status = XQC_CS_SLOW_START;
    switch_ctx->ctx_CCA = XQC_COPA;
    switch_ctx->ctx_future_CCA = XQC_CCA_NUM;
    switch_ctx->ctx_last_use_time = xqc_monotonic_timestamp();
    xqc_memzero(&switch_ctx->ctx_metric_cwnd_down_sampler, sizeof(xqc_stream_CCA_info_sampler_t));
    xqc_memzero(&switch_ctx->ctx_metric_cwnd_up_sampler, sizeof(xqc_stream_CCA_info_sampler_t));
    switch_ctx->ctx_cwnd_change = 1;
    switch_ctx->max_metric = 1;
  } else {
    xqc_switch_pq_elem_t *top_elem = xqc_switch_pq_top(switch_pq);
    switch_ctx->ctx_CCA = top_elem->switch_CCA;
  }
  
  xqc_extra_log(conn->log, conn->CS_extra_log, "[slow start begin]");
  xqc_extra_log(conn->log, conn->CS_extra_log, "[enable CS:%d]", send_ctl->ctl_conn->conn_settings.enable_CCA_switching);
  return XQC_OK;
}

void xqc_switch_ctx_enter_probe_CCA(xqc_send_ctl_t *send_ctl) {
  xqc_switch_ctx_t *switch_ctx = send_ctl->ctl_switch_ctx;
  switch_ctx->ctx_status = XQC_CS_PROBE_CCA;
}

void xqc_switch_ctx_enter_stable_running(xqc_send_ctl_t *send_ctl) {
  xqc_switch_ctx_t *switch_ctx = send_ctl->ctl_switch_ctx;
  switch_ctx->ctx_status = XQC_CS_STABLE_RUNNING;
  switch_ctx->ctx_future_CCA = XQC_CCA_NUM;
}

void xqc_switch_ctx_destroy(xqc_send_ctl_t *send_ctl)
{
  xqc_switch_ctx_t *switch_ctx = send_ctl->ctl_switch_ctx;
  xqc_switch_pq_t *switch_pq = switch_ctx->ctx_CCA_queue;
  xqc_ip_CCA_info_t *CCA_infos = xqc_ip_get_CCA_info(send_ctl);

  /* save CCA_info of current using CCA */
  int CCA_id = switch_ctx->ctx_CCA;
  CCA_infos[CCA_id].CCA_cnt = CCA_time_arr[CCA_id];
  if (send_ctl->ctl_conn->conn_settings.enable_CCA_switching) {
    for (int i = 0; i < XQC_CCA_NUM - 1; i++) {
      xqc_switch_pq_elem_t *elem = xqc_switch_pq_element(switch_pq, i);
      CCA_id = elem->switch_CCA;
      CCA_infos[CCA_id].CCA_cnt = CCA_time_arr[CCA_id];
    }
  }
  
  for (size_t i = 0; i < XQC_CCA_NUM; i++) {
    xqc_extra_log(send_ctl->ctl_conn->log, send_ctl->ctl_conn->CS_extra_log, "[CCA:%s] [used cnt:%d]", cong_ctrl_str[i], CCA_infos[i].CCA_cnt);
    xqc_log(send_ctl->ctl_conn->log, XQC_LOG_INFO, "|CCA switching|save CCA info|CCA id:%d|used cnt:%d|", i, CCA_infos[i].CCA_cnt);
  }
  
  // // free pcc proteus
  // if (send_ctl->ctl_conn->conn_settings.enable_CCA_switching) {
  //   cong_ctrl_arr[XQC_PCC]->xqc_cong_ctl_destroy(send_ctl->ctl_cong_stack[XQC_PCC]);
  // }
  
  xqc_switch_pq_destroy(switch_ctx->ctx_CCA_queue);
  xqc_free(switch_ctx->ctx_CCA_queue);

  if (send_ctl->ctl_conn->conn_settings.enable_CCA_switching) {
    if (send_ctl->ctl_metric_free_resource_cb)
      send_ctl->ctl_metric_free_resource_cb();
  }
  
}

void xqc_switch_CCA_implement(xqc_send_ctl_t *send_ctl)
{
  xqc_switch_ctx_t *switch_ctx = send_ctl->ctl_switch_ctx;
  xqc_switch_pq_t *switch_pq = switch_ctx->ctx_CCA_queue;
  xqc_connection_t *conn = send_ctl->ctl_conn;
  /* compare the current power and the max power in switch_pq */
  float cur_metric = send_ctl->ctl_switch_ctx->ctx_metric;
  float max_metric = 0.0;
  xqc_usec_t now = xqc_monotonic_timestamp();
  
  xqc_switch_pq_elem_t *top_elem = xqc_switch_pq_top(switch_pq);
  if (top_elem) {
    max_metric = top_elem->metric;
  } else {
    xqc_log(send_ctl->ctl_conn->log, XQC_LOG_ERROR, "|CCA switching|no CCA in switch_pq|");
  }

  /* rebuild pq */
  // xqc_switch_pq_rebuild(switch_ctx->ctx_CCA_queue);
  /* if max_power > cur_power, switch the CCA */
  if (!switch_ctx->can_use_coward && switch_ctx->is_weak) {
    xqc_switch_pq_push(switch_pq, cur_metric, switch_ctx->ctx_CCA, now);
    switch_ctx->ctx_CCA = XQC_COWARD;
    switch_ctx->ctx_future_CCA = XQC_CCA_NUM;
    switch_ctx->ctx_cgnum = 0;
    xqc_CCA_make_in_step(send_ctl, XQC_COWARD);
    send_ctl->ctl_cong_callback = cong_ctrl_arr[XQC_COWARD];
    send_ctl->ctl_cong = send_ctl->ctl_cong_stack[XQC_COWARD];
    xqc_extra_log(send_ctl->ctl_conn->log, send_ctl->ctl_conn->CS_extra_log, "[change to coward]");
  } else if (top_elem) {
    xqc_switch_CCA_t future_CCA = top_elem->switch_CCA;
    if (switch_ctx->ctx_cgnum >= XQC_SWITCH_THRES) {
      switch_ctx->ctx_cgnum = 0;
      /* maintain the switch_pq */
      xqc_switch_CCA_t switch_CCA = top_elem->switch_CCA;

      /* update time */ 
      CCA_time_arr[switch_ctx->ctx_CCA] += (xqc_monotonic_timestamp() - switch_ctx->ctx_last_use_time);
      switch_ctx->ctx_last_use_time = xqc_monotonic_timestamp();

      /* switch */
      send_ctl->ctl_cong_callback = cong_ctrl_arr[future_CCA];
      send_ctl->ctl_cong = send_ctl->ctl_cong_stack[future_CCA];
      xqc_extra_log(send_ctl->ctl_conn->log, send_ctl->ctl_conn->CS_extra_log, "[origin_CCA:%s] [origin_metric:%.6f] [current_CCA:%s] [current_metric:%.6f]"
          , cong_ctrl_str[switch_ctx->ctx_CCA], cur_metric, cong_ctrl_str[switch_CCA], max_metric);
      xqc_log(send_ctl->ctl_conn->log, XQC_LOG_INFO, "|CCA switching|switch CCA from %s to %s|", cong_ctrl_str[switch_ctx->ctx_CCA], cong_ctrl_str[switch_CCA]);

      /* update switch pq */ 
      xqc_switch_pq_pop(switch_pq);
      if (switch_ctx->ctx_status != XQC_CS_SLOW_START && (switch_ctx->ctx_CCA != XQC_COWARD || switch_ctx->can_use_coward)) {
        xqc_switch_pq_push(switch_pq, cur_metric, switch_ctx->ctx_CCA, now);
      } 
      if (switch_ctx->ctx_status == XQC_CS_SLOW_START) {
        xqc_switch_ctx_enter_probe_CCA(send_ctl);
      }
      switch_ctx->ctx_CCA = future_CCA;
      switch_ctx->ctx_future_CCA = XQC_CCA_NUM;
    } else {
      if (switch_ctx->ctx_future_CCA == XQC_CCA_NUM) {
        switch_ctx->ctx_future_CCA = future_CCA;
        xqc_CCA_make_in_step(send_ctl, future_CCA);
      }
      
      xqc_log(send_ctl->ctl_conn->log, XQC_LOG_INFO, "|CCA switching|CCA:%s|current cgnum:%d|", cong_ctrl_str[switch_ctx->ctx_CCA], switch_ctx->ctx_cgnum);
      xqc_extra_log(send_ctl->ctl_conn->log, send_ctl->ctl_conn->CS_extra_log, "[CCA:%s] [current cgnum:%d]", cong_ctrl_str[switch_ctx->ctx_CCA], switch_ctx->ctx_cgnum);
      switch_ctx->ctx_cgnum++;
    }
  }
}

void xqc_switch_CCA(xqc_send_ctl_t *send_ctl) 
{
  xqc_switch_ctx_t *switch_ctx = send_ctl->ctl_switch_ctx;
  uint64_t cur_time = xqc_monotonic_timestamp();
  const char *status[] = {"SLOW START", "PROBE CCA", "STABLE RUNNING"};
  xqc_extra_log(send_ctl->ctl_conn->log, send_ctl->ctl_conn->CS_extra_log, "[status:%s]", status[switch_ctx->ctx_status]);
  xqc_stream_CCA_info_sampler_t *metric_cwnd_up_sampler = &switch_ctx->ctx_metric_cwnd_up_sampler;
  xqc_stream_CCA_info_sampler_t *metric_cwnd_down_sampler = &switch_ctx->ctx_metric_cwnd_down_sampler;
  
  /* update samples */
  xqc_update_CCA_info_sample(&send_ctl->stream_CCA_info, send_ctl);
  /* update metric */
  if (send_ctl->ctl_metric_cwnd_up_cb) {
    float cur_metric_up = send_ctl->ctl_metric_cwnd_up_cb(&send_ctl->stream_CCA_info);      
    switch_ctx->ctx_metric_cwnd_up = cur_metric_up;
    xqc_stream_info_sample_once(metric_cwnd_up_sampler, cur_metric_up);
    xqc_extra_log(send_ctl->ctl_conn->log, send_ctl->ctl_conn->CS_extra_log, "[CCA:%s] [metric_up:%.6f]", cong_ctrl_str[send_ctl->ctl_switch_ctx->ctx_CCA], cur_metric_up);
  }
  if (send_ctl->ctl_metric_cwnd_down_cb) {
    float cur_metric_down = send_ctl->ctl_metric_cwnd_down_cb(&send_ctl->stream_CCA_info);      
    switch_ctx->ctx_metric_cwnd_down = cur_metric_down;
    xqc_stream_info_sample_once(metric_cwnd_down_sampler, cur_metric_down);
    xqc_extra_log(send_ctl->ctl_conn->log, send_ctl->ctl_conn->CS_extra_log, "[CCA:%s] [metric_down:%.6f]", cong_ctrl_str[send_ctl->ctl_switch_ctx->ctx_CCA], cur_metric_down);
  }
  if (send_ctl->ctl_metric_sum_up_cb) {
    switch_ctx->ctx_metric = send_ctl->ctl_metric_sum_up_cb(switch_ctx->ctx_metric_cwnd_up, switch_ctx->ctx_metric_cwnd_down, 0);
  }
  xqc_extra_log(send_ctl->ctl_conn->log, send_ctl->ctl_conn->CS_extra_log, "[CCA:%s] [metric:%.6f]", cong_ctrl_str[send_ctl->ctl_switch_ctx->ctx_CCA], switch_ctx->ctx_metric);
  /* update cwnd */
  uint64_t cur_cwnd = send_ctl->ctl_cong_callback->xqc_cong_ctl_get_cwnd(send_ctl->ctl_cong);
  switch_ctx->ctx_cwnd_change *= switch_ctx->ctx_last_cwnd != 0 ? (cur_cwnd * 1.0) / (switch_ctx->ctx_last_cwnd * 1.0) : 1.0;
  switch_ctx->ctx_last_cwnd = cur_cwnd;
  /* recover metric */
  // xqc_switch_pq_add_metric_each(switch_ctx->ctx_CCA_queue, METRIC_RECOVER, switch_ctx->max_metric);
  /* print */
  xqc_switch_pq_print(switch_ctx->ctx_CCA_queue, send_ctl->ctl_conn);
  /* CCA switching */
  if (send_ctl->ctl_conn->conn_settings.enable_CCA_switching) {
    xqc_switch_status_t switch_status = switch_ctx->ctx_status;
    xqc_switch_CCA_t switch_CCA = switch_ctx->ctx_CCA;
    xqc_bool_t is_up_sample_done = xqc_is_stream_info_sample_once_done(send_ctl, metric_cwnd_up_sampler);
    xqc_bool_t is_down_sample_done = xqc_is_stream_info_sample_once_done(send_ctl, metric_cwnd_down_sampler);
    if (is_up_sample_done || is_down_sample_done) {
      xqc_stream_info_get_result(metric_cwnd_up_sampler);
      xqc_stream_info_get_result(metric_cwnd_down_sampler);
    }

    xqc_cong_ctrl_callback_t *cong_ctrl = cong_ctrl_arr[switch_CCA];
    void *cong = send_ctl->ctl_cong_stack[switch_CCA];
    if (switch_status == XQC_CS_SLOW_START) {
      if (!cong_ctrl->xqc_cong_ctl_in_slow_start(cong)) {
        xqc_switch_CCA_implement(send_ctl);
      }
    } else if (is_up_sample_done || is_down_sample_done) {
      xqc_extra_log(send_ctl->ctl_conn->log, send_ctl->ctl_conn->CS_extra_log, "[cwnd:%.6f]", switch_ctx->ctx_cwnd_change);
      const char *status_name[] = {"XQC_CCA_NEED_CHANGE", "XQC_CCA_WAIT_FOR_EVALUATE", "XQC_CCA_CAN_STABLE_RUNNING"};
      xqc_CCA_status_t CCA_status = xqc_evaluate_CCA(send_ctl, switch_status);
      xqc_extra_log(send_ctl->ctl_conn->log, send_ctl->ctl_conn->CS_extra_log, "[CCA:%s] [status:%s]", cong_ctrl_str[switch_CCA], status_name[CCA_status+1]);
      switch_ctx->ctx_cg_status_num += CCA_status;
      if (switch_ctx->ctx_cg_status_sample_cnt >= CCA_SAMPLE_CHANGE) {
        if (switch_ctx->ctx_cg_status_num > CCA_SAMPLE_CHANGE / 2) {    
          if (switch_status == XQC_CS_PROBE_CCA) {
            xqc_switch_ctx_enter_stable_running(send_ctl);
          }
        } else if (switch_ctx->ctx_cg_status_num < -CCA_SAMPLE_CHANGE / 2) {
          if (switch_status == XQC_CS_PROBE_CCA) {
            xqc_switch_CCA_implement(send_ctl);
          } else if (switch_status == XQC_CS_STABLE_RUNNING) {
            xqc_switch_ctx_enter_probe_CCA(send_ctl);
          }
        }
        switch_ctx->ctx_cg_status_num = 0;
        switch_ctx->ctx_cg_status_sample_cnt = 0;
      } else {
        switch_ctx->ctx_cg_status_sample_cnt++;
      }
      switch_ctx->ctx_cwnd_change = 1;
    }
    
  }
}

xqc_cong_ctrl_callback_t *xqc_switch_get_future_cc_cb(xqc_send_ctl_t *send_ctl) {
  xqc_switch_CCA_t future_CCA = send_ctl->ctl_switch_ctx->ctx_future_CCA;
  xqc_cong_ctrl_callback_t *cong_ctrl = NULL;
  if (future_CCA != XQC_CCA_NUM) {
    cong_ctrl = cong_ctrl_arr[future_CCA];
  }
  return cong_ctrl;
}

xqc_CCA_status_t xqc_evaluate_CCA(xqc_send_ctl_t *send_ctl, xqc_switch_status_t switch_status) {
  xqc_switch_ctx_t *switch_ctx = send_ctl->ctl_switch_ctx;
  xqc_stream_CCA_info_t *stream_CCA_info = &send_ctl->stream_CCA_info;
  /* log */
  const char *trace_name[] = {"NONE", "UP", "DOWN"};
  xqc_connection_t *conn = send_ctl->ctl_conn;
  /* evaluate cwnd */
  float metric_cwnd_down_thres = 0, metric_cwnd_up_thres = 0;
  if (send_ctl->ctl_metric_cwnd_up_thres_cb) {
    metric_cwnd_up_thres = send_ctl->ctl_metric_cwnd_up_thres_cb(stream_CCA_info);
  }
  if (send_ctl->ctl_metric_cwnd_down_thres_cb) {
    metric_cwnd_down_thres = send_ctl->ctl_metric_cwnd_down_thres_cb(stream_CCA_info);
  }
  xqc_bool_t can_be_weak = xqc_CCA_can_be_anomaly(switch_ctx->ctx_metric_cwnd_down_sampler.avg, metric_cwnd_down_thres);
  switch_ctx->is_weak = can_be_weak;
  xqc_bool_t can_be_aggressive = xqc_CCA_can_be_anomaly(switch_ctx->ctx_metric_cwnd_up_sampler.avg, metric_cwnd_up_thres);
  xqc_extra_log(conn->log, conn->CS_extra_log, "[down_thres:%.6f] [up_thres:%.6f]", metric_cwnd_down_thres, metric_cwnd_up_thres);
  xqc_extra_log(conn->log, conn->CS_extra_log, "[cwnd_up_delta:%.6f] [cwnd_up_avg:%.6f]", switch_ctx->ctx_metric_cwnd_up_sampler.delta, switch_ctx->ctx_metric_cwnd_up_sampler.avg);
  xqc_extra_log(conn->log, conn->CS_extra_log, "[cwnd_down_delta:%.6f] [cwnd_down_avg:%.6f]", switch_ctx->ctx_metric_cwnd_down_sampler.delta, switch_ctx->ctx_metric_cwnd_down_sampler.avg);
  float cwnd_thres_down = can_be_weak ? 0.6 : 1.0;
  float cwnd_thres_up   = can_be_aggressive ? 1.2 : 1.0;
  xqc_extra_log(conn->log, conn->CS_extra_log, "[can_be_weak:%s] [can_be_aggressive:%s]", can_be_weak ? "true" : "false", can_be_aggressive ? "true" : "false");
  xqc_CCA_info_trace_type_t cwnd_trace = xqc_evaluate_CCA_trace_eval_helper(switch_ctx->ctx_cwnd_change, cwnd_thres_up, cwnd_thres_down);
  xqc_extra_log(conn->log, conn->CS_extra_log, "[cwnd trace:%s]", trace_name[cwnd_trace]);
  xqc_CCA_status_t CCA_status = XQC_CCA_WAIT_FOR_EVALUATE;
  xqc_bool_t reason = xqc_CCA_check_metric_decrease_reason(switch_ctx, stream_CCA_info, cwnd_trace, can_be_weak, can_be_aggressive);
  

  if (switch_status == XQC_CS_PROBE_CCA) {
    CCA_status = reason ? XQC_CCA_CAN_STABLE_RUNNING : XQC_CCA_NEED_CHANGE;
  } else if (switch_status == XQC_CS_STABLE_RUNNING) {
    CCA_status = XQC_CCA_CAN_STABLE_RUNNING;
    if (can_be_weak) {
      if (switch_ctx->ctx_metric_cwnd_down_sampler.delta <= 0) {
        CCA_status = XQC_CCA_NEED_CHANGE;
      }
    } else if (can_be_aggressive) {
      if (switch_ctx->ctx_metric_cwnd_up_sampler.delta <= 0) {
        CCA_status = XQC_CCA_NEED_CHANGE;
      }
    }
  }

  if (send_ctl->ctl_cong_callback->xqc_cong_ctl_on_switching) {
    send_ctl->ctl_cong_callback->xqc_cong_ctl_on_switching(send_ctl->ctl_cong, !can_be_weak || switch_ctx->ctx_metric_cwnd_down_sampler.delta > 3);
    if (can_be_weak) {
      CCA_status = XQC_CCA_CAN_STABLE_RUNNING;
    } else {
      if (!switch_ctx->can_use_coward) {
        CCA_status = XQC_CCA_NEED_CHANGE;
      }
    }
  }
  return CCA_status;
}

xqc_bool_t
xqc_CCA_check_metric_decrease_reason(xqc_switch_ctx_t *switch_ctx, xqc_stream_CCA_info_t *stream_CCA_info, xqc_CCA_info_trace_type_t cwnd_trace,
           xqc_bool_t can_be_weak, xqc_bool_t can_be_aggressive) {
  // xqc_connection_t *conn = switch_ctx->ctx_send_ctl->ctl_conn;
  xqc_bool_t trace_eval = XQC_TRUE;
  xqc_bool_t has_set_aggressive = XQC_FALSE;
  if (switch_ctx->ctx_metric_cwnd_up_sampler.delta < 0 || can_be_aggressive) {
    trace_eval = cwnd_trace == XQC_CCA_INFO_TRACE_UP;
    has_set_aggressive = XQC_TRUE;
  }
  if ((switch_ctx->ctx_metric_cwnd_down_sampler.delta < 0 && !has_set_aggressive) || can_be_weak) {
    if (can_be_weak) {
      trace_eval = cwnd_trace == XQC_CCA_INFO_TRACE_DOWN;
    } else {
      trace_eval = cwnd_trace != XQC_CCA_INFO_TRACE_UP;
    }
  }
  return trace_eval;
}

void xqc_CCA_make_in_step(xqc_send_ctl_t *send_ctl, xqc_switch_CCA_t future)
{
  uint64_t pacing_rate, cwnd;
  xqc_cong_ctrl_callback_t *cong_cb = send_ctl->ctl_cong_callback;
  void *cong = send_ctl->ctl_cong;
  if (cong_cb->xqc_cong_ctl_get_cwnd)
  {
    pacing_rate = xqc_pacing_rate_calc(&send_ctl->ctl_pacing);
    cwnd = cong_cb->xqc_cong_ctl_get_cwnd(cong);
  }
  if (cong_cb->xqc_cong_ctl_get_pacing_rate)
  {
    pacing_rate = cong_cb->xqc_cong_ctl_get_pacing_rate(cong);
  }
  cong_cb = cong_ctrl_arr[future];
  cong = send_ctl->ctl_cong_stack[future];
  if (cong_cb->xqc_cong_ctl_set_cwnd)
  {
    cong_cb->xqc_cong_ctl_set_cwnd(cong, cwnd);
  }
  if (cong_cb->xqc_cong_ctl_set_pacing_rate)
  {
    cong_cb->xqc_cong_ctl_set_pacing_rate(cong, pacing_rate);
  }
  if (cong_cb->xqc_cong_ctl_set_rtt)
  {
    cong_cb->xqc_cong_ctl_set_rtt(cong, send_ctl->ctl_minrtt, send_ctl->ctl_srtt);
  }
}