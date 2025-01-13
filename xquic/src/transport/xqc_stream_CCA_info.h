// added by jndu
#ifndef XQC_STREAM_CCA_INFO_H
#define XQC_STREAM_CCA_INFO_H

#include <xquic/xquic_typedef.h>

typedef struct xqc_stream_CCA_info_s {
  uint64_t throughput;
  uint64_t max_throughput;
  uint64_t loss_rate;
  uint64_t max_delivary_rate;
  uint64_t delivery_rate;
  uint64_t latest_rtt;
  uint64_t min_rtt;
} xqc_stream_CCA_info_t;

/**
 * @brief pass updated value to CCA sampler
 */
void xqc_update_CCA_info_sample(xqc_stream_CCA_info_t *CCA_sampler, xqc_send_ctl_t *send_ctl);

/**
 * @brief public API for get CCA info (throughput, rtt, etc.)
 */
uint64_t xqc_get_CCA_info_sample(xqc_stream_CCA_info_t *CCA_sampler, xqc_CCA_info_sample_type_t type);

static inline float 
xqc_get_EWMA_value(float before, float after) {
  float alpha = 0.3;
  return alpha * after + (1 - alpha) * before;
}

#endif