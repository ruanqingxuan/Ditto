#include <xquic/xquic_typedef.h>
#include "src/transport/xqc_send_ctl.h"
#include "src/congestion_control/xqc_sample.h"
#include "src/transport/xqc_stream_CCA_info.h"
#include "src/common/xqc_extra_log.h"

void xqc_update_CCA_info_sample(xqc_stream_CCA_info_t *stream_CCA_info, xqc_send_ctl_t *send_ctl)
{
  xqc_sample_t *sampler = &send_ctl->sampler;
  xqc_CCA_info_trace_type_t trace_type;
  /* delivery_rate */
  uint64_t cur_delivery_rate = xqc_get_EWMA_value(stream_CCA_info->delivery_rate, sampler->delivery_rate);
  stream_CCA_info->delivery_rate = cur_delivery_rate;
  stream_CCA_info->max_delivary_rate = xqc_max(stream_CCA_info->delivery_rate, stream_CCA_info->max_delivary_rate);
  /* latest rtt */
  stream_CCA_info->min_rtt = send_ctl->ctl_minrtt;
  stream_CCA_info->latest_rtt = send_ctl->ctl_srtt;
  /* loss rate */
  uint64_t cur_loss_rate = xqc_get_EWMA_value(stream_CCA_info->loss_rate, send_ctl->ctl_loss_rate * 10000000);
  stream_CCA_info->loss_rate = cur_loss_rate;
  /* throughput */
  uint64_t cur_throughput = xqc_get_EWMA_value(stream_CCA_info->throughput, send_ctl->ctl_throughput * 10000000);
  stream_CCA_info->max_throughput = send_ctl->ctl_max_throughput * 10000000;
  stream_CCA_info->throughput = cur_throughput;
  
  xqc_log(send_ctl->ctl_conn->log, XQC_LOG_INFO, "|CCA switching|delivery_rate: %ud|latest_rtt: %ud|loss_rate: %ud"
          "|max_throughput: %ud|throughput: %ud|min_rtt: %ud|", stream_CCA_info->delivery_rate, stream_CCA_info->latest_rtt,
          stream_CCA_info->loss_rate, stream_CCA_info->max_throughput, stream_CCA_info->throughput,
          stream_CCA_info->min_rtt);
  xqc_extra_log(send_ctl->ctl_conn->log, send_ctl->ctl_conn->CS_extra_log,"[delivery_rate: %ud] [latest_rtt: %ud] [loss_rate: %ud] "
          "[max_throughput: %ud] [throughput: %ud] [min_rtt: %ud]", stream_CCA_info->delivery_rate, stream_CCA_info->latest_rtt,
          stream_CCA_info->loss_rate, stream_CCA_info->max_throughput, stream_CCA_info->throughput,
          stream_CCA_info->min_rtt);
}

uint64_t xqc_get_CCA_info_sample(xqc_stream_CCA_info_t *stream_CCA_info, xqc_CCA_info_sample_type_t type)
{
  uint64_t res = 0;
  switch (type) {
    case XQC_CCA_INFO_SAMPLE_THROUGHPUT:
      res = stream_CCA_info->throughput;
      break;
    case XQC_CCA_INFO_SAMPLE_MAX_THROUGHPUT:
      res = stream_CCA_info->max_throughput;
      break;
    case XQC_CCA_INFO_SAMPLE_LOSS_RATE:
      res = stream_CCA_info->loss_rate;
      break;
    case XQC_CCA_INFO_SAMPLE_LATEST_RTT:
      res = stream_CCA_info->latest_rtt;
      break;
    case XQC_CCA_INFO_SAMPLE_MAX_DELIVERY_RATE:
      res = stream_CCA_info->max_delivary_rate;
      break;
    case XQC_CCA_INFO_SAMPLE_DELIVERY_RATE:
      res = stream_CCA_info->delivery_rate;
      break;
    case XQC_CCA_INFO_SAMPLE_MIN_RTT:
      res = stream_CCA_info->min_rtt;
    default:
      break;
  }
  return res;
}


void xqc_notice_CCA_info_metric_calc_change(xqc_stream_CCA_info_t *stream_CCA_info, const char *notice) {
  xqc_connection_t *conn = stream_CCA_info->send_ctl->ctl_conn;
  xqc_extra_log(conn->log, conn->CS_extra_log, notice);
  /* pop switch_pq */
  xqc_switch_ctx_t *switch_ctx = stream_CCA_info->send_ctl->ctl_switch_ctx;
  xqc_switch_pq_t *switch_pq = switch_ctx->ctx_CCA_queue;
  switch_ctx->max_metric = stream_CCA_info->send_ctl->ctl_metric_sum_up_cb(1, 1, 0);
  xqc_switch_pq_add_metric_each(switch_pq, switch_ctx->max_metric, switch_ctx->max_metric);
  
}

xqc_bool_t xqc_is_stream_info_sample_once_done(xqc_send_ctl_t *send_ctl, xqc_stream_CCA_info_sampler_t *sampler) {
  xqc_bool_t can_evaluate = (sampler->cnt >= MAX_SAMPLE);
  if (send_ctl->ctl_metric_notice_can_evaluate_cb) {
    can_evaluate = send_ctl->ctl_metric_notice_can_evaluate_cb(&send_ctl->stream_CCA_info);
  }
  return can_evaluate;
}
