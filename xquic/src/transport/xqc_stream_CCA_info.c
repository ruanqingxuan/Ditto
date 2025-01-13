#include <xquic/xquic_typedef.h>
#include "src/transport/xqc_send_ctl.h"
#include "src/congestion_control/xqc_sample.h"
#include "src/transport/xqc_stream_CCA_info.h"
#include "src/common/xqc_extra_log.h"

void xqc_update_CCA_info_sample(xqc_stream_CCA_info_t *CCA_sampler, xqc_send_ctl_t *send_ctl)
{
  xqc_sample_t *sampler = &send_ctl->sampler;
  CCA_sampler->delivery_rate = xqc_get_EWMA_value(CCA_sampler->delivery_rate, sampler->delivery_rate);
  CCA_sampler->max_delivary_rate = xqc_max(CCA_sampler->delivery_rate, CCA_sampler->max_delivary_rate);
  CCA_sampler->latest_rtt = send_ctl->ctl_srtt;
  CCA_sampler->loss_rate = xqc_get_EWMA_value(CCA_sampler->loss_rate, send_ctl->ctl_loss_rate * 10000000);
  CCA_sampler->max_throughput = send_ctl->ctl_max_throughput * 10000000;
  CCA_sampler->throughput = xqc_get_EWMA_value(CCA_sampler->throughput, send_ctl->ctl_throughput * 10000000);
  CCA_sampler->min_rtt = send_ctl->ctl_minrtt;
  xqc_log(send_ctl->ctl_conn->log, XQC_LOG_INFO, "|CCA switching|delivery_rate: %ud|latest_rtt: %ud|loss_rate: %ud"
          "|max_throughput: %ud|throughput: %ud|min_rtt: %ud|", CCA_sampler->delivery_rate, CCA_sampler->latest_rtt,
          CCA_sampler->loss_rate, CCA_sampler->max_throughput, CCA_sampler->throughput,
          CCA_sampler->min_rtt);
  xqc_extra_log(send_ctl->ctl_conn->log, send_ctl->ctl_conn->CS_extra_log,"[delivery_rate: %ud] [latest_rtt: %ud] [loss_rate: %ud] "
          "[max_throughput: %ud] [throughput: %ud] [min_rtt: %ud]", CCA_sampler->delivery_rate, CCA_sampler->latest_rtt,
          CCA_sampler->loss_rate, CCA_sampler->max_throughput, CCA_sampler->throughput,
          CCA_sampler->min_rtt);
}

uint64_t xqc_get_CCA_info_sample(xqc_stream_CCA_info_t *CCA_sampler, xqc_CCA_info_sample_type_t type)
{
  uint64_t res = 0;
  switch (type) {
    case XQC_CCA_INFO_SAMPLE_THROUGHPUT:
      res = CCA_sampler->throughput;
      break;
    case XQC_CCA_INFO_SAMPLE_MAX_THROUGHPUT:
      res = CCA_sampler->max_throughput;
      break;
    case XQC_CCA_INFO_SAMPLE_LOSS_RATE:
      res = CCA_sampler->loss_rate;
      break;
    case XQC_CCA_INFO_SAMPLE_LATEST_RTT:
      res = CCA_sampler->latest_rtt;
      break;
    case XQC_CCA_INFO_SAMPLE_MAX_DELIVERY_RATE:
      res = CCA_sampler->max_delivary_rate;
      break;
    case XQC_CCA_INFO_SAMPLE_DELIVERY_RATE:
      res = CCA_sampler->delivery_rate;
      break;
    case XQC_CCA_INFO_SAMPLE_MIN_RTT:
      res = CCA_sampler->min_rtt;
    default:
      break;
  }
  return res;
}