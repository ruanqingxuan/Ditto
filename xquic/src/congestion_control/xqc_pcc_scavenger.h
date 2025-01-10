/**
 * @copyright Copyright (c) 2024, ANTS Group Holding Limited
 */

#include <xquic/xquic.h>
#include <xquic/xquic_typedef.h>
#include "src/congestion_control/xqc_window_filter.h"

#define MAX_HISTORY_SIZE 100

typedef enum
{
    // Initial phase of the connection. Sending rate gets doubled as
    // long as utility keeps increasing, and the sender enters
    // PROBING mode when utility decreases.
    PCC_STARTING,
    // Sender tries different sending rates to decide whether higher
    // or lower sending rate has greater utility. Sender enters
    // DECISION_MADE mode once a decision is made.
    PCC_PROBING,
    // Sender keeps increasing or decreasing sending rate until
    // utility decreases, then sender returns to PROBING mode.
    // TODO(tongmeng): a better name?
    PCC_DECISION_MADE,
} xqc_pcc_mode;

// Indicates whether sender should increase or decrease sending rate.
typedef enum
{
    INCREASE,
    DECREASE
} xqc_pcc_direction_t;

typedef struct xqc_pcc_monitor_s
{
    bool burst_flag;
    // EWMA of ratio between two consecutive ACK intervals, i.e., interval between
    // reception time of two consecutive ACKs.
    float avg_interval_ratio;
    // Number of useful intervals in the queue.
    size_t num_useful_intervals;
    // Number of useful intervals in the queue with available utilities.
    size_t num_available_intervals;
} xqc_pcc_monitor_t;

typedef struct xqc_pcc_interval_stats_s
{
    float interval_duration;
    float rtt_ratio;
    int64_t marked_lost_bytes;
    float loss_rate;
    float actual_sending_rate_mbps;
    float ack_rate_mbps;

    float avg_rtt;
    float rtt_dev;
    float min_rtt;
    float max_rtt;
    float approx_rtt_gradient;

    float rtt_gradient;
    float rtt_gradient_cut;
    float rtt_gradient_error;

    float trending_gradient;
    float trending_gradient_cut;
    float trending_gradient_error;

    float trending_deviation;
} xqc_pcc_interval_stats_t;

typedef struct xqc_pcc_utility_s
{
    // String tag that represents the utility function.
    char utility_tag[256]; // Fixed size array for a string (replace 256 with appropriate size)
    // May be different from actual utility tag when using Hybrid utility.
    char effective_utility_tag_[256]; // Same as above
    // Parameters needed by some utility functions, e.g., sending rate bound used
    // in hybrid utility functions.
    void **utility_parameters;

    // Performance metrics for latest monitor interval.
    xqc_pcc_interval_stats_t interval_stats;

    size_t lost_bytes_tolerance_quota;

    float avg_mi_rtt_dev;
    float dev_mi_rtt_dev;
    float min_rtt;

    float mi_avg_rtt_history_[MAX_HISTORY_SIZE]; // MI RTT history
    float avg_trending_gradient;
    float min_trending_gradient;
    float dev_trending_gradient;
    float last_trending_gradient;

    float mi_rtt_dev_history_[MAX_HISTORY_SIZE]; // MI RTT deviation history
    float avg_trending_dev;
    float min_trending_dev;
    float dev_trending_dev;
    float last_trending_dev;

    float ratio_inflated_mi;
    float ratio_fluctuated_mi;

    bool is_rtt_inflation_tolerable;
    bool is_rtt_dev_tolerable;
} xqc_pcc_utility_t;

typedef struct xqc_pcc_s
{
    /* Current mode */
    xqc_pcc_mode mode;
    /* State of the sender */
    xqc_send_ctl_t *send_ctl;
    // Most recent utility used when making the last rate change decision.
    float latest_utility;
    // Duration of the current monitor interval.
    xqc_usec_t monitor_duration;
    // Current direction of rate changes.
    xqc_pcc_direction_t direction;
    // Number of rounds sender remains in current mode.
    size_t rounds_;
    // Queue of monitor intervals with pending utilities.
    xqc_pcc_monitor_t interval_queue;
    // Smoothed RTT before consecutive inflated RTTs happen.
    xqc_usec_t rtt_on_inflation_start;
    // Maximum congestion window in bytes, used to cap sending rate.
    uint64_t max_cwnd_bytes;
    xqc_usec_t rtt_deviation;
    xqc_usec_t min_rtt_deviation;
    xqc_usec_t latest_rtt;
    xqc_usec_t min_rtt;
    xqc_usec_t avg_rtt;
    // Latched value of FLAGS_exit_starting_based_on_sampled_bandwidth.
    const bool exit_starting_based_on_sampled_bandwidth;
    // Time when the most recent packet is sent.
    xqc_usec_t latest_sent_timestamp;
    // Time when the most recent ACK is received.
    xqc_usec_t latest_ack_timestamp;
    // Utility manager is responsible for utility calculation.
    xqc_pcc_utility_t utility_manager;

} xqc_pcc_t;

/* added by qnwang */
extern xqc_cong_ctrl_callback_t xqc_pcc_cb;