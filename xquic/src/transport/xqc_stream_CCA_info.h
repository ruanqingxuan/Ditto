// added by jndu
#ifndef XQC_STREAM_CCA_INFO_H
#define XQC_STREAM_CCA_INFO_H

#include <xquic/xquic_typedef.h>

#define DELIVERY_RATE_CHANGE 5000
#define RTT_CHANGE 500
#define THROUGHPUT_CHANGE 5000000
#define LOSS_RATE_CHANGE 5000

#define MAX_SAMPLE 7

typedef struct xqc_stream_CCA_info_sampler_s
{
  float avg;
  float win[MAX_SAMPLE];
  int cnt;
  uint64_t last_sample_time;
  float delta;
} xqc_stream_CCA_info_sampler_t;

typedef struct xqc_stream_CCA_info_s
{
  xqc_send_ctl_t *send_ctl;
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
void xqc_update_CCA_info_sample(xqc_stream_CCA_info_t *stream_CCA_info, xqc_send_ctl_t *send_ctl);

/**
 * @brief public API for get CCA info (throughput, rtt, etc.)
 */
uint64_t xqc_get_CCA_info_sample(xqc_stream_CCA_info_t *stream_CCA_info, xqc_CCA_info_sample_type_t type);

/**
 * @brief public API for noticing the change of metric calc
 */
void xqc_notice_CCA_info_metric_calc_change(xqc_stream_CCA_info_t *stream_CCA_info, const char *notice);

static inline float
xqc_get_EWMA_value(float before, float after)
{
  float alpha = 0.3;
  return alpha * after + (1 - alpha) * before;
}

static inline xqc_CCA_info_trace_type_t
xqc_evaluate_CCA_trace_eval_helper(float trace_num, float thres_up, float thres_down)
{
  xqc_CCA_info_trace_type_t trace_type = XQC_CCA_INFO_TRACE_NONE;
  if (trace_num > thres_up)
  {
    trace_type = XQC_CCA_INFO_TRACE_UP;
  }
  else if (trace_num < thres_down)
  {
    trace_type = XQC_CCA_INFO_TRACE_DOWN;
  }
  return trace_type;
}

static inline int
xqc_evaluate_CCA_trace_add_helper(xqc_CCA_info_trace_type_t trace_type)
{
  int res = 0;
  switch (trace_type)
  {
  case XQC_CCA_INFO_TRACE_UP:
    res = 1;
    break;
  case XQC_CCA_INFO_TRACE_DOWN:
    res = -1;
    break;
  default:
    break;
  }
  return res;
}

static inline void
xqc_stream_info_sample_once(xqc_stream_CCA_info_sampler_t *sampler, float value)
{

  if (sampler->last_sample_time == 0)
  {
    sampler->last_sample_time = xqc_monotonic_timestamp();
  }

  if (sampler->cnt < MAX_SAMPLE)
  {
    sampler->win[sampler->cnt++] = value;
  }
}

static inline void
xqc_stream_info_get_result(xqc_stream_CCA_info_sampler_t *sampler)
{
  static float weight[MAX_SAMPLE];
  for (int i = 0; i < MAX_SAMPLE; i++)
  {
    weight[i] = 1.0;
  }

  float sum_up = 0, sum_down = 0;
  for (int i = 0; i < sampler->cnt; i++)
  {
    sum_up += (sampler->win[i] * weight[i]);
    sum_down += weight[i];
  }
  uint64_t now = xqc_monotonic_timestamp();
  float avg = sum_up * 1.0 / sum_down * 1.0;
  sampler->delta = (avg - sampler->avg) / 1.0 * (now - sampler->last_sample_time);
  sampler->avg = avg;
  sampler->last_sample_time = now;
  sampler->cnt = 0;
}

xqc_bool_t xqc_is_stream_info_sample_once_done(xqc_send_ctl_t *send_ctl, xqc_stream_CCA_info_sampler_t *sampler);

#endif