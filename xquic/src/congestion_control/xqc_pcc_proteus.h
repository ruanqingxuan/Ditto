// added by jndu
// based on PCC proteus

#ifndef _XQC_PCC_H_INCLUDED_
#define _XQC_PCC_H_INCLUDED_

#include "xquic/xquic.h"
#include "xquic/xquic_typedef.h"
#include "src/common/xqc_array.h"
#include "src/common/xqc_queue.h"
#include "src/common/xqc_str.h"


/**
 * @brief based on pcc_monitor_interval_queue.h
 */
typedef struct PacketRttSample
{
    uint64_t packet_number;
    uint64_t sample_rtt;
    uint64_t ack_timestamp;
    xqc_bool_t is_reliable;
    xqc_bool_t is_reliable_for_gradient_calculation;
} PacketRttSample;

typedef struct LostPacketSample
{
    uint64_t packet_number;
    uint64_t bytes;
} LostPacketSample;

typedef struct AckedPacket
{
    uint64_t packet_number;
    uint64_t bytes_acked;
    uint64_t receive_timestamp;
} AckedPacket;

typedef struct LostPacket
{
    uint64_t packet_number;
    uint64_t bytes_lost;
} LostPacket;

typedef struct MonitorInterval
{
    // Sending rate.
    uint64_t sending_rate;
    // True if calculating utility for this MonitorInterval.
    xqc_bool_t is_useful;
    // The tolerable rtt fluctuation ratio.
    float rtt_fluctuation_tolerance_ratio;

    // Sent time of the first packet.
    uint64_t first_packet_sent_time;
    // Sent time of the last packet.
    uint64_t last_packet_sent_time;

    // PacketNumber of the first sent packet.
    uint64_t first_packet_number;
    // PacketNumber of the last sent packet.
    uint64_t last_packet_number;

    // Number of bytes which are sent in total.
    uint64_t bytes_sent;
    // Number of bytes which have been acked.
    uint64_t bytes_acked;
    // Number of bytes which are considered as lost.
    uint64_t bytes_lost;

    // Smoothed RTT when the first packet is sent.
    uint64_t rtt_on_monitor_start;
    // RTT when all sent packets are either acked or lost.
    uint64_t rtt_on_monitor_end;
    // Minimum RTT seen by PCC sender.
    uint64_t min_rtt;

    // Interval since previous sent packet for each packet in the interval.
    // type uint64_t
    xqc_array_t *packet_sent_intervals;
    // Packet RTT sample for each sent packet in the monitor interval.
    // type PacketRttSample
    xqc_array_t *packet_rtt_samples;
    // Lost packet sample for each lost packet in the monitor interval.
    // type LostPacketSample
    xqc_array_t *lost_packet_samples;

    size_t num_reliable_rtt;
    size_t num_reliable_rtt_for_gradient_calculation;
    // True if the interval has enough number of reliable RTT samples.
    xqc_bool_t has_enough_reliable_rtt;

    // True only if the monitor duration is doubled due to lack of reliable RTTs.
    xqc_bool_t is_monitor_duration_extended;
} MonitorInterval;

#define MONITOR_INTERVAL_SAMPLE_CAPACITY 5

static inline MonitorInterval *xqc_monitor_interval_create(uint64_t sending_rate, xqc_bool_t is_useful, float rtt_fluctuation_tolerance_ratio, uint64_t rtt)
{
    MonitorInterval *monitor_interval = (MonitorInterval *)xqc_malloc(sizeof(MonitorInterval));
    monitor_interval->sending_rate = sending_rate;
    monitor_interval->is_useful = is_useful;
    monitor_interval->rtt_fluctuation_tolerance_ratio = rtt_fluctuation_tolerance_ratio;
    monitor_interval->first_packet_sent_time = 0;
    monitor_interval->last_packet_sent_time = 0;
    monitor_interval->first_packet_number = 0;
    monitor_interval->last_packet_number = 0;
    monitor_interval->bytes_sent = 0;
    monitor_interval->bytes_acked = 0;
    monitor_interval->bytes_lost = 0;
    monitor_interval->rtt_on_monitor_start = rtt;
    monitor_interval->rtt_on_monitor_end = rtt;
    monitor_interval->min_rtt = rtt;
    monitor_interval->num_reliable_rtt = 0;
    monitor_interval->num_reliable_rtt_for_gradient_calculation = 0;
    monitor_interval->has_enough_reliable_rtt = XQC_FALSE;
    monitor_interval->is_monitor_duration_extended = XQC_FALSE;

    // init vectors
    monitor_interval->packet_sent_intervals = xqc_array_create(xqc_default_allocator, MONITOR_INTERVAL_SAMPLE_CAPACITY, sizeof(uint64_t));
    monitor_interval->packet_rtt_samples = xqc_array_create(xqc_default_allocator, MONITOR_INTERVAL_SAMPLE_CAPACITY, sizeof(PacketRttSample));
    monitor_interval->lost_packet_samples = xqc_array_create(xqc_default_allocator, MONITOR_INTERVAL_SAMPLE_CAPACITY, sizeof(LostPacketSample));
    return monitor_interval;
}

static inline void xqc_monitor_interval_destroy(MonitorInterval *monitor_interval)
{
    xqc_array_destroy(monitor_interval->packet_sent_intervals);
    xqc_array_destroy(monitor_interval->packet_rtt_samples);
    xqc_array_destroy(monitor_interval->lost_packet_samples);
    xqc_free(monitor_interval);
}

typedef struct MonitorIntervalDeque
{
    MonitorInterval *monitorInterval;
    xqc_queue_t queue;
} MonitorIntervalDeque;

static inline void PushBackMonitorIntervalDeque(xqc_queue_t *q, MonitorInterval *monitor_interval)
{
    MonitorIntervalDeque *monitor_interval_deque = (MonitorIntervalDeque *)xqc_malloc(sizeof(MonitorIntervalDeque));
    monitor_interval_deque->monitorInterval = monitor_interval;
    monitor_interval_deque->queue.next = &monitor_interval_deque->queue;
    monitor_interval_deque->queue.prev = &monitor_interval_deque->queue;
    xqc_queue_insert_tail(q, &monitor_interval_deque->queue);
}

static inline void ClearMonitorIntervalDeque(xqc_queue_t *q)
{
    xqc_queue_t *pos, *next;
    for (pos = q->next; pos != q; pos = next)
    {
        MonitorIntervalDeque *monitor_interval_deque = xqc_queue_data(pos, MonitorIntervalDeque, queue);
        MonitorInterval *interval = monitor_interval_deque->monitorInterval;
        xqc_monitor_interval_destroy(interval);
        next = pos->next;
        xqc_free(monitor_interval_deque);
    }
    xqc_queue_init(q);
}

// declaration
typedef struct xqc_pcc_proteus_s xqc_pcc_proteus_t;

typedef struct PccMonitorIntervalQueue
{
    xqc_queue_t monitor_intervals_;
    size_t monitor_intervals_size_;
    // Vector of acked packets with pending RTT reliability.
    xqc_array_t *pending_acked_packets_;
    // Latest RTT corresponding to pending acked packets.
    uint64_t pending_rtt_;
    // Average RTT corresponding to pending acked packets.
    uint64_t pending_avg_rtt_;
    // ACK interval corresponding to pending acked packets.
    uint64_t pending_ack_interval_;
    // ACK reception time corresponding to pending acked packets.
    uint64_t pending_event_time_;

    xqc_bool_t burst_flag_;

    // EWMA of ratio between two consecutive ACK intervals, i.e., interval between
    // reception time of two consecutive ACKs.
    float avg_interval_ratio_;

    // Number of useful intervals in the queue.
    size_t num_useful_intervals_;
    // Number of useful intervals in the queue with available utilities.
    size_t num_available_intervals_;
    // Delegate interface, not owned.
    xqc_pcc_proteus_t *delegate_;
} PccMonitorIntervalQueue;

static inline PccMonitorIntervalQueue *xqc_pcc_monitor_interval_queue_create(xqc_pcc_proteus_t *pcc)
{
    PccMonitorIntervalQueue *pcc_monitor_interval_queue = (PccMonitorIntervalQueue *)xqc_malloc(sizeof(PccMonitorIntervalQueue));

    pcc_monitor_interval_queue->pending_rtt_ = 0;
    pcc_monitor_interval_queue->pending_avg_rtt_ = 0;
    pcc_monitor_interval_queue->pending_ack_interval_ = 0;
    pcc_monitor_interval_queue->pending_event_time_ = 0;
    pcc_monitor_interval_queue->burst_flag_ = XQC_FALSE;
    pcc_monitor_interval_queue->avg_interval_ratio_ = -1.0;
    pcc_monitor_interval_queue->num_useful_intervals_ = 0;
    pcc_monitor_interval_queue->num_available_intervals_ = 0;
    pcc_monitor_interval_queue->delegate_ = pcc;
    pcc_monitor_interval_queue->monitor_intervals_size_ = 0;

    // init monitor interval
    xqc_queue_init(&pcc_monitor_interval_queue->monitor_intervals_);

    // init pending_acked_packets_
    pcc_monitor_interval_queue->pending_acked_packets_ = xqc_array_create(xqc_default_allocator, MONITOR_INTERVAL_SAMPLE_CAPACITY, sizeof(AckedPacket));
    return pcc_monitor_interval_queue;
}

static inline void xqc_pcc_monitor_interval_queue_destroy(PccMonitorIntervalQueue *pcc_monitor_interval_queue)
{
    xqc_array_destroy(pcc_monitor_interval_queue->pending_acked_packets_);
    ClearMonitorIntervalDeque(&pcc_monitor_interval_queue->monitor_intervals_);
}

void EnqueueNewMonitorIntervalPccMonitorIntervalQueue(PccMonitorIntervalQueue *pcc_monitor_interval_queue, uint64_t sending_rate, xqc_bool_t is_useful, float rtt_fluctuation_tolerance_ratio, uint64_t rtt);

void OnPacketSentPccMonitorIntervalQueue(PccMonitorIntervalQueue *pcc_monitor_interval_queue, uint64_t sent_time, uint64_t packet_number, uint64_t bytes, uint64_t sent_interval);

void OnCongestionEventPccMonitorIntervalQueue(PccMonitorIntervalQueue *pcc_monitor_interval_queue, const xqc_array_t *acked_packets, const xqc_array_t *lost_packets, uint64_t avg_rtt, uint64_t latest_rtt, uint64_t min_rtt, uint64_t event_time, uint64_t ack_interval);

void OnRttInflationInStartingPccMonitorIntervalQueue(PccMonitorIntervalQueue *pcc_monitor_interval_queue);

MonitorInterval *frontPccMonitorIntervalQueue(PccMonitorIntervalQueue *pcc_monitor_interval_queue);

MonitorInterval *currentPccMonitorIntervalQueue(PccMonitorIntervalQueue *pcc_monitor_interval_queue);

void extend_current_intervalPccMonitorIntervalQueue(PccMonitorIntervalQueue *pcc_monitor_interval_queue);

static inline size_t num_useful_intervalsPccMonitorIntervalQueue(PccMonitorIntervalQueue *pcc_monitor_interval_queue) { return pcc_monitor_interval_queue->num_useful_intervals_; }

static inline size_t num_available_intervalsPccMonitorIntervalQueue(PccMonitorIntervalQueue *pcc_monitor_interval_queue) { return pcc_monitor_interval_queue->num_available_intervals_; }

xqc_bool_t emptyPccMonitorIntervalQueue(PccMonitorIntervalQueue *pcc_monitor_interval_queue);

size_t sizePccMonitorIntervalQueue(PccMonitorIntervalQueue *pcc_monitor_interval_queue);

// Returns true if the utility of |interval| is available, i.e.,
// when all the interval's packets are either acked or lost.
xqc_bool_t IsUtilityAvailablePccMonitorIntervalQueue(PccMonitorIntervalQueue *pcc_monitor_interval_queue, const MonitorInterval *interval);

// Retruns true if |packet_number| belongs to |interval|.
xqc_bool_t IntervalContainsPacketPccMonitorIntervalQueue(PccMonitorIntervalQueue *pcc_monitor_interval_queue, const MonitorInterval *interval,
                                                         uint64_t packet_number);

// Returns true if the utility of |interval| is invalid, i.e., if it only
// contains a single sent packet.
xqc_bool_t HasInvalidUtilityPccMonitorIntervalQueue(PccMonitorIntervalQueue *pcc_monitor_interval_queue, const MonitorInterval *interval);

/**
 * @brief based on pcc_utility_manager.h
 */

typedef struct IntervalStats
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
} IntervalStats;

static inline IntervalStats xqc_interval_stats_create()
{
    IntervalStats interval_stats;
    xqc_memzero(&interval_stats, sizeof(IntervalStats));
    interval_stats.min_rtt = -1;
    interval_stats.max_rtt = -1;
    return interval_stats;
}

typedef struct MiAvgRttHistoryQueue
{
    float mi_avg_rtt;
    xqc_queue_t queue;
} MiAvgRttHistoryQueue;

static inline void PushBackMiAvgRttHistoryQueue(xqc_queue_t *q, float val)
{
    MiAvgRttHistoryQueue *mi_avg_rtt_history_queue = (MiAvgRttHistoryQueue *)xqc_malloc(sizeof(MiAvgRttHistoryQueue));
    mi_avg_rtt_history_queue->mi_avg_rtt = val;
    mi_avg_rtt_history_queue->queue.next = &mi_avg_rtt_history_queue->queue;
    mi_avg_rtt_history_queue->queue.prev = &mi_avg_rtt_history_queue->queue;
    xqc_queue_insert_tail(q, &mi_avg_rtt_history_queue->queue);
}

static inline void ClearMiAvgRttHistoryQueue(xqc_queue_t *q)
{
    xqc_queue_t *pos, *next;
    for (pos = q->next; pos != q; pos = next)
    {
        MiAvgRttHistoryQueue *mi_avg_rtt_history_queue = xqc_queue_data(pos, MiAvgRttHistoryQueue, queue);
        next = pos->next;
        xqc_free(mi_avg_rtt_history_queue);
    }
    xqc_queue_init(q);
}

typedef struct MiRttDevHistoryQueue
{
    float mi_rtt_dev;
    xqc_queue_t queue;
} MiRttDevHistoryQueue;

static inline void PushBackMiRttDevHistoryQueue(xqc_queue_t *q, float val)
{
    MiRttDevHistoryQueue *mi_rtt_dev_history_queue = (MiRttDevHistoryQueue *)xqc_malloc(sizeof(MiRttDevHistoryQueue));
    mi_rtt_dev_history_queue->mi_rtt_dev = val;
    mi_rtt_dev_history_queue->queue.next = &mi_rtt_dev_history_queue->queue;
    mi_rtt_dev_history_queue->queue.prev = &mi_rtt_dev_history_queue->queue;
    xqc_queue_insert_tail(q, &mi_rtt_dev_history_queue->queue);
}

static inline void ClearMiRttDevHistoryQueue(xqc_queue_t *q)
{
    xqc_queue_t *pos, *next;
    for (pos = q->next; pos != q; pos = next)
    {
        MiRttDevHistoryQueue *mi_rtt_dev_history_queue = xqc_queue_data(pos, MiRttDevHistoryQueue, queue);
        next = pos->next;
        xqc_free(mi_rtt_dev_history_queue);
    }
    xqc_queue_init(q);
}

typedef struct PccUtilityManager
{
    // only need scavenger
    // String tag that represents the utility function.
    // xqc_str_t utility_tag_;
    // May be different from actual utility tag when using Hybrid utility.
    // xqc_str_t effective_utility_tag_;

    // Parameters needed by some utility functions, e.g., sending rate bound used
    // in hybrid utility functions.
    xqc_array_t *utility_parameters_;

    // Performance metrics for latest monitor interval.
    IntervalStats interval_stats_;

    size_t lost_bytes_tolerance_quota_;

    float avg_mi_rtt_dev_;
    float dev_mi_rtt_dev_;
    float min_rtt_;

    xqc_queue_t mi_avg_rtt_history_;
    size_t mi_avg_rtt_history_size_;
    float avg_trending_gradient_;
    float min_trending_gradient_;
    float dev_trending_gradient_;
    float last_trending_gradient_;

    xqc_queue_t mi_rtt_dev_history_;
    size_t mi_rtt_dev_history_size_;

    float avg_trending_dev_;
    float min_trending_dev_;
    float dev_trending_dev_;
    float last_trending_dev_;

    float ratio_inflated_mi_;
    float ratio_fluctuated_mi_;

    xqc_bool_t is_rtt_inflation_tolerable_;
    xqc_bool_t is_rtt_dev_tolerable_;

    xqc_pcc_proteus_t *delegate_;
} PccUtilityManager;

PccUtilityManager *xqc_pcc_utility_manager_create(xqc_pcc_proteus_t *delegate_);

static inline void xqc_pcc_utility_manager_destroy(PccUtilityManager *pcc_utility_manager)
{
    xqc_array_destroy(pcc_utility_manager->utility_parameters_);
    ClearMiAvgRttHistoryQueue(&pcc_utility_manager->mi_avg_rtt_history_);
    ClearMiRttDevHistoryQueue(&pcc_utility_manager->mi_rtt_dev_history_);
}

// Utility calculation interface for all pcc senders.
float CalculateUtilityPccUtilityManager(PccUtilityManager *pcc_utility_manager, const MonitorInterval *interval, uint64_t event_time);

// Set the parameter needed by utility function.
void SetUtilityParameterPccUtilityManager(PccUtilityManager *pcc_utility_manager, void *param);

// Get the specified utility parameter.
void *GetUtilityParameterPccUtilityManager(PccUtilityManager *pcc_utility_manager, int parameter_index);

// Prepare performance metrics for utility calculation.
void PrepareStatisticsPccUtilityManager(PccUtilityManager *pcc_utility_manager, const MonitorInterval *interval);
void PreProcessingPccUtilityManager(PccUtilityManager *pcc_utility_manager, const MonitorInterval *interval);
void ComputeSimpleMetricsPccUtilityManager(PccUtilityManager *pcc_utility_manager, const MonitorInterval *interval);
void ComputeApproxRttGradientPccUtilityManager(PccUtilityManager *pcc_utility_manager, const MonitorInterval *interval);
void ComputeRttGradientPccUtilityManager(PccUtilityManager *pcc_utility_manager, const MonitorInterval *interval);
void ComputeRttGradientErrorPccUtilityManager(PccUtilityManager *pcc_utility_manager, const MonitorInterval *);
void ComputeRttDeviationPccUtilityManager(PccUtilityManager *pcc_utility_manager, const MonitorInterval *interval);

void ProcessRttTrendPccUtilityManager(PccUtilityManager *pcc_utility_manager, const MonitorInterval *interval);
void ComputeTrendingGradientPccUtilityManager(PccUtilityManager *pcc_utility_manager);
void ComputeTrendingGradientErrorPccUtilityManager(PccUtilityManager *pcc_utility_manager);
void ComputeTrendingDeviationPccUtilityManager(PccUtilityManager *pcc_utility_manager);

void DetermineToleranceGeneralPccUtilityManager(PccUtilityManager *pcc_utility_manager);
void DetermineToleranceInflationPccUtilityManager(PccUtilityManager *pcc_utility_manager);
void DetermineToleranceDeviationPccUtilityManager(PccUtilityManager *pcc_utility_manager);

float CalculateUtilityScavengerPccUtilityManager(PccUtilityManager *pcc_utility_manager, const MonitorInterval *interval,
                                                 float rtt_variance_coefficient);

/**
 * @brief based on pcc_sender.h and pcc_vivace_sender.h
 */

typedef struct UtilityInfo
{
    uint64_t sending_rate;
    float utility;
} UtilityInfo;

// Sender's mode during a connection.
typedef enum SenderMode
{
    // Initial phase of the connection. Sending rate gets doubled as
    // long as utility keeps increasing, and the sender enters
    // PROBING mode when utility decreases.
    STARTING,
    // Sender tries different sending rates to decide whether higher
    // or lower sending rate has greater utility. Sender enters
    // DECISION_MADE mode once a decision is made.
    PROBING,
    // Sender keeps increasing or decreasing sending rate until
    // utility decreases, then sender returns to PROBING mode.
    // TODO(tongmeng): a better name?
    DECISION_MADE
} SenderMode;

// Indicates whether sender should increase or decrease sending rate.
typedef enum RateChangeDirection
{
    INCREASE,
    DECREASE
} RateChangeDirection;

typedef struct xqc_pcc_proteus_s
{
    xqc_send_ctl_t *send_ctl;
    // Current mode of PccSender.
    SenderMode mode_;
    // Sending rate for the next monitor intervals.
    uint64_t sending_rate_;
    // Initialized to be false, and set to true after receiving the first ACK.
    xqc_bool_t has_seen_valid_rtt_;
    // Most recent utility used when making the last rate change decision.
    float latest_utility_;

    uint64_t conn_start_time_;

    // Duration of the current monitor interval.
    uint64_t monitor_duration_;
    // Current direction of rate changes.
    RateChangeDirection direction_;
    // Number of rounds sender remains in current mode.
    size_t rounds_;
    // Queue of monitor intervals with pending utilities.
    PccMonitorIntervalQueue *interval_queue_;

    // Smoothed RTT before consecutive inflated RTTs happen.
    uint64_t rtt_on_inflation_start_;

    // Maximum congestion window in bytes, used to cap sending rate.
    uint64_t max_cwnd_bytes_;

    uint64_t rtt_deviation_;
    uint64_t min_rtt_deviation_;
    uint64_t latest_rtt_;
    uint64_t min_rtt_;
    uint64_t avg_rtt_;
    // const QuicUnackedPacketMap* unacked_packets_;
    // QuicRandom* random_;

    // Bandwidth sample provides the bandwidth measurement that is used when
    // exiting STARTING phase upon early termination.
    // BandwidthSampler sampler_;
    // Filter that tracks maximum bandwidth over multiple recent round trips.
    // MaxBandwidthFilter max_bandwidth_;
    // Packet number for the most recently sent packet.
    // QuicPacketNumber last_sent_packet_;
    // Largest packet number that is sent in current round trips.
    // QuicPacketNumber current_round_trip_end_;
    // Number of round trips since connection start.
    // QuicRoundTripCount round_trip_count_;
    // Latched value of FLAGS_exit_starting_based_on_sampled_bandwidth.
    xqc_bool_t exit_starting_based_on_sampled_bandwidth_;

    // Time when the most recent packet is sent.
    uint64_t latest_sent_timestamp_;
    // Time when the most recent ACK is received.
    uint64_t latest_ack_timestamp_;

    // Utility manager is responsible for utility calculation.
    PccUtilityManager *utility_manager_;

    /**
     * pcc_vivace_sender.h
     */
    // Most recent utility info used when making the last rate change decision.
    UtilityInfo latest_utility_info_;

    // Number of incremental rate change step size allowed on basis of initial
    // maximum rate change step size.
    size_t incremental_rate_change_step_allowance_;
} xqc_pcc_proteus_t;

void OnCongestionEventSender(xqc_pcc_proteus_t *pcc, xqc_bool_t rtt_updated, uint64_t rtt,
                             uint64_t bytes_in_flight,
                             uint64_t event_time,
                             const xqc_array_t *acked_packets, const xqc_array_t *lost_packets);

void OnPacketSentSender(xqc_pcc_proteus_t *pcc, uint64_t sent_time,
                        uint64_t bytes_in_flight,
                        uint64_t packet_number,
                        uint64_t bytes,
                        xqc_bool_t is_retransmittable);

xqc_bool_t CanSendSender(xqc_pcc_proteus_t *pcc, uint64_t bytes_in_flight);

uint64_t PacingRateSender(xqc_pcc_proteus_t *pcc, uint64_t bytes_in_flight);

uint64_t GetCongestionWindowSender(xqc_pcc_proteus_t *pcc);

void OnUtilityAvailableSender(xqc_pcc_proteus_t *pcc,
                              const xqc_array_t *useful_intervals,
                              uint64_t event_time);

size_t GetNumIntervalGroupsInProbingSender(xqc_pcc_proteus_t *pcc);

void SetUtilityParameterSender(xqc_pcc_proteus_t *pcc, void *param);

void UpdateRttSender(xqc_pcc_proteus_t *pcc, uint64_t event_time, uint64_t rtt);
// friend class test::PccSenderPeer;
/*typedef WindowedFilter<QuicBandwidth,
                       MaxFilter<QuicBandwidth>,
                       QuicRoundTripCount,
                       QuicRoundTripCount>
    MaxBandwidthFilter;*/

// Returns true if a new monitor interval needs to be created.
xqc_bool_t CreateNewIntervalSender(xqc_pcc_proteus_t *pcc, uint64_t event_time);
// Returns true if next created monitor interval is useful,
// i.e., its utility will be used when a decision can be made.
xqc_bool_t CreateUsefulIntervalSender(xqc_pcc_proteus_t *pcc);
// Returns the sending rate for non-useful monitor interval.
uint64_t GetSendingRateForNonUsefulIntervalSender(xqc_pcc_proteus_t *pcc);
// Maybe set sending_rate_ for next created monitor interval.
void MaybeSetSendingRateSender(xqc_pcc_proteus_t *pcc);
// Returns the max RTT fluctuation tolerance according to sender mode.
float GetMaxRttFluctuationToleranceSender(xqc_pcc_proteus_t *pcc);

// Set sending rate to central probing rate for the coming round of PROBING.
void RestoreCentralSendingRateSender(xqc_pcc_proteus_t *pcc);
// Returns true if the sender can enter DECISION_MADE from PROBING mode.
xqc_bool_t CanMakeDecisionSenderVivace(xqc_pcc_proteus_t *pcc, const xqc_array_t *utility_info);

// Set the sending rate to the central rate used in PROBING mode.
void EnterProbingSenderVivace(xqc_pcc_proteus_t *pcc, const xqc_array_t *utility_info);
// Set the sending rate when entering DECISION_MADE from PROBING mode.
void EnterDecisionMadeSenderVivace(xqc_pcc_proteus_t *pcc, const xqc_array_t *utility_info);

void EnterProbingSender(xqc_pcc_proteus_t *pcc);
void EnterDecisionMadeSender(xqc_pcc_proteus_t *pcc);
uint64_t ComputeRateChangeSenderVivace(xqc_pcc_proteus_t *pcc, const xqc_array_t *utility_info);
void SetRateChangeDirectionSenderVivace(xqc_pcc_proteus_t *pcc, const xqc_array_t *utility_info);
// Returns true if the RTT inflation is larger than the tolerance.
xqc_bool_t CheckForRttInflationSender(xqc_pcc_proteus_t *pcc);

extern xqc_cong_ctrl_callback_t xqc_pcc_proteus_cb;

#endif