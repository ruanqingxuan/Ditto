#include "src/congestion_control/xqc_pcc_proteus.h"
#include "src/transport/xqc_packet_out.h"
#include "src/congestion_control/xqc_sample.h"
#include "src/common/xqc_time.h"
#include "src/common/xqc_config.h"
#include "src/common/xqc_extra_log.h"
#include "src/transport/xqc_send_ctl.h"
#include <xquic/xquic.h>
#include <xquic/xquic_typedef.h>
#include <math.h>
#include <stdio.h>

/**
 * based on pcc_monitor_interval_queue.cpp
 */
const size_t kMinReliableRtt = 4;

void EnqueueNewMonitorIntervalPccMonitorIntervalQueue(PccMonitorIntervalQueue *pcc_monitor_interval_queue, uint64_t sending_rate, xqc_bool_t is_useful, float rtt_fluctuation_tolerance_ratio, uint64_t rtt)
{
    xqc_pcc_proteus_t *pcc = pcc_monitor_interval_queue->delegate_;
    xqc_connection_t *conn = pcc->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    if (is_useful)
    {
        pcc_monitor_interval_queue->num_useful_intervals_++;
    }

    MonitorInterval *monitor_interval = xqc_monitor_interval_create(sending_rate, is_useful, rtt_fluctuation_tolerance_ratio, rtt);

    PushBackMonitorIntervalDeque(&pcc_monitor_interval_queue->monitor_intervals_, monitor_interval);
    pcc_monitor_interval_queue->monitor_intervals_size_++;
}

void OnPacketSentPccMonitorIntervalQueue(PccMonitorIntervalQueue *pcc_monitor_interval_queue, uint64_t sent_time, uint64_t packet_number, uint64_t bytes, uint64_t sent_interval)
{
    xqc_pcc_proteus_t *pcc = pcc_monitor_interval_queue->delegate_;
    xqc_connection_t *conn = pcc->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    xqc_queue_t *monitor_interval_q = &pcc_monitor_interval_queue->monitor_intervals_;
    if (xqc_queue_empty(monitor_interval_q))
    {
        return;
    }

    MonitorIntervalDeque *monitor_interval_deque_back = xqc_queue_data(xqc_queue_tail(monitor_interval_q), MonitorIntervalDeque, queue);
    MonitorInterval *monitor_interval_back = monitor_interval_deque_back->monitorInterval;
    if (monitor_interval_back->bytes_sent == 0)
    {
        monitor_interval_back->first_packet_sent_time = sent_time;
        monitor_interval_back->first_packet_number = packet_number;
    }

    monitor_interval_back->last_packet_sent_time = sent_time;
    monitor_interval_back->last_packet_number = packet_number;
    monitor_interval_back->bytes_sent += bytes;
    xqc_array_push_back(monitor_interval_back->packet_sent_intervals, &sent_interval);
}

void OnCongestionEventPccMonitorIntervalQueue(PccMonitorIntervalQueue *pcc_monitor_interval_queue, const xqc_array_t *acked_packets,
                                              const xqc_array_t *lost_packets, uint64_t avg_rtt, uint64_t latest_rtt, uint64_t min_rtt, uint64_t event_time, uint64_t ack_interval)
{
    xqc_pcc_proteus_t *pcc = pcc_monitor_interval_queue->delegate_;
    xqc_connection_t *conn = pcc->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    pcc_monitor_interval_queue->num_available_intervals_ = 0;
    if (pcc_monitor_interval_queue->num_useful_intervals_ == 0)
    {
        return;
    }

    xqc_bool_t has_invalid_utility = XQC_FALSE;

    xqc_queue_t *pos;
    xqc_queue_foreach(pos, &pcc_monitor_interval_queue->monitor_intervals_)
    {
        MonitorIntervalDeque *monitor_interval_deque = xqc_queue_data(pos, MonitorIntervalDeque, queue);
        MonitorInterval *interval = monitor_interval_deque->monitorInterval;
        if (!interval->is_useful)
        {
            // Skips useless monitor intervals.
            continue;
        }

        if (IsUtilityAvailablePccMonitorIntervalQueue(pcc_monitor_interval_queue, interval))
        {
            // Skip intervals with available utilities.
            ++pcc_monitor_interval_queue->num_available_intervals_;
            continue;
        }

        for (int i = 0; i < lost_packets->size; i++)
        {
            LostPacket *lost_packet = xqc_array_get(lost_packets, i);
            if (IntervalContainsPacketPccMonitorIntervalQueue(pcc_monitor_interval_queue, interval, lost_packet->packet_number))
            {
                interval->bytes_lost += lost_packet->bytes_lost;
                LostPacketSample lost_packet_sample = {lost_packet->packet_number, lost_packet->bytes_lost};
                xqc_array_push_back(interval->lost_packet_samples, &lost_packet_sample);
            }
        }

        for (int i = 0; i < pcc_monitor_interval_queue->pending_acked_packets_->size; i++)
        {
            AckedPacket *acked_packet = xqc_array_get(pcc_monitor_interval_queue->pending_acked_packets_, i);
            if (IntervalContainsPacketPccMonitorIntervalQueue(pcc_monitor_interval_queue, interval, acked_packet->packet_number))
            {
                if (interval->bytes_acked == 0)
                {
                    // This is the RTT before starting sending at interval.sending_rate.
                    interval->rtt_on_monitor_start = pcc_monitor_interval_queue->pending_avg_rtt_;
                }
                interval->bytes_acked += acked_packet->bytes_acked;

                xqc_bool_t is_reliable = XQC_FALSE;
                if (pcc_monitor_interval_queue->pending_ack_interval_ != 0)
                {
                    float interval_ratio =
                        (float)(pcc_monitor_interval_queue->pending_ack_interval_) /
                        (float)(ack_interval);
                    if (interval_ratio < 1.0)
                    {
                        interval_ratio = 1.0 / interval_ratio;
                    }
                    if (pcc_monitor_interval_queue->avg_interval_ratio_ < 0)
                    {
                        pcc_monitor_interval_queue->avg_interval_ratio_ = interval_ratio;
                    }

                    if (interval_ratio > 50.0 * pcc_monitor_interval_queue->avg_interval_ratio_)
                    {
                        pcc_monitor_interval_queue->burst_flag_ = XQC_TRUE;
                    }
                    else if (pcc_monitor_interval_queue->burst_flag_)
                    {
                        if (latest_rtt > pcc_monitor_interval_queue->pending_rtt_ && pcc_monitor_interval_queue->pending_rtt_ < pcc_monitor_interval_queue->pending_avg_rtt_)
                        {
                            pcc_monitor_interval_queue->burst_flag_ = XQC_FALSE;
                        }
                    }
                    else
                    {
                        is_reliable = XQC_TRUE;
                        interval->num_reliable_rtt++;
                    }

                    pcc_monitor_interval_queue->avg_interval_ratio_ =
                        pcc_monitor_interval_queue->avg_interval_ratio_ * 0.9 + interval_ratio * 0.1;
                }

                xqc_bool_t is_reliable_for_gradient_calculation = XQC_FALSE;
                if (is_reliable)
                {
                    // if (latest_rtt > pending_rtt_) {
                    is_reliable_for_gradient_calculation = XQC_TRUE;
                    interval->num_reliable_rtt_for_gradient_calculation++;
                }

                PacketRttSample packet_rtt_sample = {acked_packet->packet_number, pcc_monitor_interval_queue->pending_rtt_,
                                                     pcc_monitor_interval_queue->pending_event_time_,
                                                     is_reliable, is_reliable_for_gradient_calculation};
                xqc_array_push_back(interval->packet_rtt_samples, &packet_rtt_sample);
                if (interval->num_reliable_rtt >= kMinReliableRtt)
                {
                    interval->has_enough_reliable_rtt = XQC_TRUE;
                }
            }
        }

        if (IsUtilityAvailablePccMonitorIntervalQueue(pcc_monitor_interval_queue, interval))
        {
            interval->rtt_on_monitor_end = avg_rtt;
            interval->min_rtt = min_rtt;
            has_invalid_utility = HasInvalidUtilityPccMonitorIntervalQueue(pcc_monitor_interval_queue, interval);
            if (has_invalid_utility)
            {
                break;
            }
            ++pcc_monitor_interval_queue->num_available_intervals_;
            assert(pcc_monitor_interval_queue->num_available_intervals_ <= pcc_monitor_interval_queue->num_useful_intervals_);
        }

    }

    xqc_array_clear(pcc_monitor_interval_queue->pending_acked_packets_);

    for (int i = 0; i < acked_packets->size; i++)
    {
        AckedPacket *acked_packet = xqc_array_get(acked_packets, i);
        xqc_array_push_back(pcc_monitor_interval_queue->pending_acked_packets_, acked_packet);
    }

    pcc_monitor_interval_queue->pending_rtt_ = latest_rtt;
    pcc_monitor_interval_queue->pending_avg_rtt_ = avg_rtt;
    pcc_monitor_interval_queue->pending_ack_interval_ = ack_interval;
    pcc_monitor_interval_queue->pending_event_time_ = event_time;

    if (pcc_monitor_interval_queue->num_useful_intervals_ > pcc_monitor_interval_queue->num_available_intervals_ &&
        !has_invalid_utility)
    {
        return;
    }

    if (!has_invalid_utility)
    {
        assert(pcc_monitor_interval_queue->num_useful_intervals_ > 0u);

        xqc_array_t *useful_intervals = xqc_array_create(xqc_default_allocator, MONITOR_INTERVAL_SAMPLE_CAPACITY, sizeof(MonitorInterval *));

        xqc_queue_t *pos;
        xqc_queue_foreach(pos, &pcc_monitor_interval_queue->monitor_intervals_)
        {
            MonitorIntervalDeque *monitor_interval_deque = xqc_queue_data(pos, MonitorIntervalDeque, queue);
            MonitorInterval *interval = monitor_interval_deque->monitorInterval;

            if (!interval->is_useful)
            {
                continue;
            }
            xqc_array_push_back(useful_intervals, &interval);
        }
        assert(pcc_monitor_interval_queue->num_available_intervals_ == useful_intervals->size);

        OnUtilityAvailableSender(pcc_monitor_interval_queue->delegate_, useful_intervals, event_time);

        xqc_array_destroy(useful_intervals);
    }

    // Remove MonitorIntervals from the head of the queue,
    // until all useful intervals are removed.
    while (pcc_monitor_interval_queue->num_useful_intervals_ > 0)
    {
        xqc_queue_t *head = xqc_queue_head(&pcc_monitor_interval_queue->monitor_intervals_);
        MonitorIntervalDeque *monitor_interval_deque_front = xqc_queue_data(head, MonitorIntervalDeque, queue);
        MonitorInterval *interval_front = monitor_interval_deque_front->monitorInterval;
        if (interval_front->is_useful)
        {
            --pcc_monitor_interval_queue->num_useful_intervals_;
        }

        xqc_queue_remove(head);
        xqc_monitor_interval_destroy(interval_front);
        xqc_free(monitor_interval_deque_front);
    }
    pcc_monitor_interval_queue->num_available_intervals_ = 0;
}

void OnRttInflationInStartingPccMonitorIntervalQueue(PccMonitorIntervalQueue *pcc_monitor_interval_queue)
{
    xqc_pcc_proteus_t *pcc = pcc_monitor_interval_queue->delegate_;
    xqc_connection_t *conn = pcc->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    ClearMonitorIntervalDeque(&pcc_monitor_interval_queue->monitor_intervals_);
    pcc_monitor_interval_queue->num_useful_intervals_ = 0;
    pcc_monitor_interval_queue->num_available_intervals_ = 0;
    pcc_monitor_interval_queue->monitor_intervals_size_ = 0;
}

MonitorInterval *frontPccMonitorIntervalQueue(PccMonitorIntervalQueue *pcc_monitor_interval_queue)
{
    xqc_pcc_proteus_t *pcc = pcc_monitor_interval_queue->delegate_;
    xqc_connection_t *conn = pcc->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    assert(!xqc_queue_empty(&pcc_monitor_interval_queue->monitor_intervals_));
    xqc_queue_t *head = xqc_queue_head(&pcc_monitor_interval_queue->monitor_intervals_);
    MonitorIntervalDeque *monitor_interval_deque_front = xqc_queue_data(head, MonitorIntervalDeque, queue);
    MonitorInterval *interval_front = monitor_interval_deque_front->monitorInterval;
    return interval_front;
}

MonitorInterval *currentPccMonitorIntervalQueue(PccMonitorIntervalQueue *pcc_monitor_interval_queue)
{
    xqc_pcc_proteus_t *pcc = pcc_monitor_interval_queue->delegate_;
    xqc_connection_t *conn = pcc->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    assert(!xqc_queue_empty(&pcc_monitor_interval_queue->monitor_intervals_));
    xqc_queue_t *tail = xqc_queue_tail(&pcc_monitor_interval_queue->monitor_intervals_);
    MonitorIntervalDeque *monitor_interval_deque_back = xqc_queue_data(tail, MonitorIntervalDeque, queue);
    MonitorInterval *interval_back = monitor_interval_deque_back->monitorInterval;
    return interval_back;
}

void extend_current_intervalPccMonitorIntervalQueue(PccMonitorIntervalQueue *pcc_monitor_interval_queue)
{
    xqc_pcc_proteus_t *pcc = pcc_monitor_interval_queue->delegate_;
    xqc_connection_t *conn = pcc->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    assert(!xqc_queue_empty(&pcc_monitor_interval_queue->monitor_intervals_));
    xqc_queue_t *tail = xqc_queue_tail(&pcc_monitor_interval_queue->monitor_intervals_);
    MonitorIntervalDeque *monitor_interval_deque_back = xqc_queue_data(tail, MonitorIntervalDeque, queue);
    MonitorInterval *interval_back = monitor_interval_deque_back->monitorInterval;
    interval_back->is_monitor_duration_extended = XQC_TRUE;
}

xqc_bool_t emptyPccMonitorIntervalQueue(PccMonitorIntervalQueue *pcc_monitor_interval_queue)
{
    xqc_pcc_proteus_t *pcc = pcc_monitor_interval_queue->delegate_;
    xqc_connection_t *conn = pcc->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    return xqc_queue_empty(&pcc_monitor_interval_queue->monitor_intervals_);
}

size_t sizePccMonitorIntervalQueue(PccMonitorIntervalQueue *pcc_monitor_interval_queue)
{
    xqc_pcc_proteus_t *pcc = pcc_monitor_interval_queue->delegate_;
    xqc_connection_t *conn = pcc->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    return pcc_monitor_interval_queue->monitor_intervals_size_;
}

// Returns XQC_TRUE if the utility of |interval| is available, i.e.,
// when all the interval's packets are either acked or lost.
xqc_bool_t IsUtilityAvailablePccMonitorIntervalQueue(PccMonitorIntervalQueue *pcc_monitor_interval_queue, const MonitorInterval *interval)
{
    xqc_pcc_proteus_t *pcc = pcc_monitor_interval_queue->delegate_;
    xqc_connection_t *conn = pcc->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    return (interval->has_enough_reliable_rtt && interval->bytes_acked + interval->bytes_lost == interval->bytes_sent);
}

// Retruns XQC_TRUE if |packet_number| belongs to |interval|.
xqc_bool_t IntervalContainsPacketPccMonitorIntervalQueue(PccMonitorIntervalQueue *pcc_monitor_interval_queue, const MonitorInterval *interval,
                                                         uint64_t packet_number)
{
    xqc_pcc_proteus_t *pcc = pcc_monitor_interval_queue->delegate_;
    xqc_connection_t *conn = pcc->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    return (packet_number >= interval->first_packet_number && packet_number <= interval->last_packet_number);
}

// Returns XQC_TRUE if the utility of |interval| is invalid, i.e., if it only
// contains a single sent packet.
xqc_bool_t HasInvalidUtilityPccMonitorIntervalQueue(PccMonitorIntervalQueue *pcc_monitor_interval_queue, const MonitorInterval *interval)
{
    xqc_pcc_proteus_t *pcc = pcc_monitor_interval_queue->delegate_;
    xqc_connection_t *conn = pcc->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    return interval->first_packet_sent_time == interval->last_packet_sent_time;
}

/**
 * based on pcc_utility_manager.cpp
 */
const uint64_t kMaxPacketSize = 1500;
// Number of bits per byte.
const size_t kBitsPerByte = 8;
const size_t kRttHistoryLen = 6;

// Tolerance of loss rate by Allegro utility function.
// Allegro 效用函数对丢失率的容忍度。
const float kLossTolerance = 0.05f;
// Coefficeint of the loss rate term in Allegro utility function.
// Allegro 效用函数中损失率项的系数。
const float kLossCoefficient = -1000.0f;
// Coefficient of RTT term in Allegro utility function.
// Allegro 效用函数中 RTT 项的系数，表示 RTT 增加对效用值的影响。
const float kRTTCoefficient = -200.0f;

// Exponent of sending rate contribution term in Vivace utility function.
const float kSendingRateExponent = 0.9f;
// Coefficient of loss penalty term in Vivace utility function.
const float kVivaceLossCoefficient = 11.35f;
// Coefficient of latency penalty term in Vivace utility function.
const float kLatencyCoefficient = 900.0f;

// Coefficient of rtt deviation term in Scavenger utility function.
const float kRttDeviationCoefficient = 0.0015f;

// The factor for sending rate transform in hybrid utility function.
const float kHybridUtilityRateTransformFactor = 0.1f;

// The update rate for moving average variable.
const float kAlpha = 0.1f;
// The order of magnitude that distinguishes abnormal sample.
const float kBeta = 100.0f;

// The threshold for ratio of monitor interval count, above which moving average
// of trending RTT metrics (gradient and deviation) would be reset.
const float kTrendingResetIntervalRatio = 0.95f;

// Number of deviation above/below average trending gradient used for RTT
// inflation tolerance for primary and scavenger senders.
const float kInflationToleranceGainHigh = 2.0f;
const float kInflationToleranceGainLow = 2.0f;

const size_t kLostPacketTolerance = 10;

PccUtilityManager *xqc_pcc_utility_manager_create(xqc_pcc_proteus_t *delegate_)
{
    PccUtilityManager *pcc_utility_manager = (PccUtilityManager *)xqc_malloc(sizeof(PccUtilityManager));
    pcc_utility_manager->lost_bytes_tolerance_quota_ = kMaxPacketSize * kLostPacketTolerance;
    pcc_utility_manager->avg_mi_rtt_dev_ = -1.0;
    pcc_utility_manager->dev_mi_rtt_dev_ = -1.0;
    pcc_utility_manager->min_rtt_ = -1.0;
    pcc_utility_manager->avg_trending_gradient_ = -1.0;
    pcc_utility_manager->min_trending_gradient_ = -1.0;
    pcc_utility_manager->dev_trending_gradient_ = -1.0;
    pcc_utility_manager->last_trending_dev_ = -1.0;
    pcc_utility_manager->avg_trending_dev_ = -1.0;
    pcc_utility_manager->min_trending_dev_ = -1.0;
    pcc_utility_manager->dev_trending_dev_ = -1.0;
    pcc_utility_manager->last_trending_dev_ = -1.0;
    pcc_utility_manager->ratio_inflated_mi_ = 0;
    pcc_utility_manager->ratio_fluctuated_mi_ = 0;
    pcc_utility_manager->is_rtt_inflation_tolerable_ = XQC_TRUE;
    pcc_utility_manager->is_rtt_dev_tolerable_ = XQC_TRUE;
    pcc_utility_manager->delegate_ = delegate_;

    // init interval stats
    pcc_utility_manager->interval_stats_ = xqc_interval_stats_create();

    // init utility parameters
    pcc_utility_manager->utility_parameters_ = xqc_array_create(xqc_default_allocator, MONITOR_INTERVAL_SAMPLE_CAPACITY, sizeof(void *));

    // init queue
    xqc_queue_init(&pcc_utility_manager->mi_avg_rtt_history_);
    xqc_queue_init(&pcc_utility_manager->mi_rtt_dev_history_);

    return pcc_utility_manager;
}

// Utility calculation interface for all pcc senders.
float CalculateUtilityPccUtilityManager(PccUtilityManager *pcc_utility_manager, const MonitorInterval *interval, uint64_t event_time)
{
    xqc_pcc_proteus_t *pcc = pcc_utility_manager->delegate_;
    xqc_connection_t *conn = pcc->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    assert(interval->first_packet_sent_time != interval->last_packet_sent_time);

    PrepareStatisticsPccUtilityManager(pcc_utility_manager, interval);
    float utility = 0.0;

    float rtt_deviation_coefficient = *(*(float **)(xqc_array_get(pcc_utility_manager->utility_parameters_, 0)));
    xqc_extra_log(conn->log, conn->CS_extra_log, "[rtt_deviation_coefficient:%.6f]", rtt_deviation_coefficient);
    utility = CalculateUtilityScavengerPccUtilityManager(pcc_utility_manager, interval, rtt_deviation_coefficient);
    return utility;
}

// Set the parameter needed by utility function.
void SetUtilityParameterPccUtilityManager(PccUtilityManager *pcc_utility_manager, void *param)
{
    xqc_pcc_proteus_t *pcc = pcc_utility_manager->delegate_;
    xqc_connection_t *conn = pcc->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    float *p = xqc_malloc(sizeof(float));
    xqc_memcpy(p, param, sizeof(float));
    xqc_array_push_back(pcc_utility_manager->utility_parameters_, &p);
}

// Get the specified utility parameter.
void *GetUtilityParameterPccUtilityManager(PccUtilityManager *pcc_utility_manager, int parameter_index)
{
    xqc_pcc_proteus_t *pcc = pcc_utility_manager->delegate_;
    xqc_connection_t *conn = pcc->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    float *default_param = (float *)xqc_malloc(sizeof(float));
    *default_param = 0.0f;
    return pcc_utility_manager->utility_parameters_->size > parameter_index
               ? *(float **)xqc_array_get(pcc_utility_manager->utility_parameters_, parameter_index)
               : default_param;
}

// Prepare performance metrics for utility calculation.
void PrepareStatisticsPccUtilityManager(PccUtilityManager *pcc_utility_manager, const MonitorInterval *interval)
{
    xqc_pcc_proteus_t *pcc = pcc_utility_manager->delegate_;
    xqc_connection_t *conn = pcc->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    PreProcessingPccUtilityManager(pcc_utility_manager, interval);

    ComputeSimpleMetricsPccUtilityManager(pcc_utility_manager, interval);
    ComputeApproxRttGradientPccUtilityManager(pcc_utility_manager, interval);
    ComputeRttGradientPccUtilityManager(pcc_utility_manager, interval);
    ComputeRttDeviationPccUtilityManager(pcc_utility_manager, interval);
    ComputeRttGradientErrorPccUtilityManager(pcc_utility_manager, interval);

    DetermineToleranceGeneralPccUtilityManager(pcc_utility_manager);
    ProcessRttTrendPccUtilityManager(pcc_utility_manager, interval);
}

void PreProcessingPccUtilityManager(PccUtilityManager *pcc_utility_manager, const MonitorInterval *interval)
{
    xqc_pcc_proteus_t *pcc = pcc_utility_manager->delegate_;
    xqc_connection_t *conn = pcc->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    pcc_utility_manager->interval_stats_.marked_lost_bytes = 0;
}

void ComputeSimpleMetricsPccUtilityManager(PccUtilityManager *pcc_utility_manager, const MonitorInterval *interval)
{
    xqc_pcc_proteus_t *pcc = pcc_utility_manager->delegate_;
    xqc_connection_t *conn = pcc->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    // Add the transfer time of the last packet in the monitor interval when
    // calculating monitor interval duration.
    pcc_utility_manager->interval_stats_.interval_duration = (float)(interval->last_packet_sent_time - interval->first_packet_sent_time + kMaxPacketSize * 8 * 1000000 / interval->sending_rate);

    pcc_utility_manager->interval_stats_.rtt_ratio = (float)(interval->rtt_on_monitor_start) / (float)(interval->rtt_on_monitor_end);
    pcc_utility_manager->interval_stats_.loss_rate = (float)(interval->bytes_lost - pcc_utility_manager->interval_stats_.marked_lost_bytes) / (float)(interval->bytes_sent);
    pcc_utility_manager->interval_stats_.actual_sending_rate_mbps = (float)(interval->bytes_sent) * (float)(kBitsPerByte) / pcc_utility_manager->interval_stats_.interval_duration;
    size_t num_rtt_samples = interval->packet_rtt_samples->size;
    if (num_rtt_samples > 1)
    {
        PacketRttSample *sample_back = xqc_array_get(interval->packet_rtt_samples, num_rtt_samples - 1);
        PacketRttSample *sample_front = xqc_array_get(interval->packet_rtt_samples, 0);
        float ack_duration = (float)(sample_back->ack_timestamp - sample_front->ack_timestamp);
        pcc_utility_manager->interval_stats_.ack_rate_mbps = (float)(interval->bytes_acked - kMaxPacketSize) * (float)(kBitsPerByte) / ack_duration;
    }
    else if (num_rtt_samples == 1)
    {
        pcc_utility_manager->interval_stats_.ack_rate_mbps = (float)(interval->bytes_acked) / pcc_utility_manager->interval_stats_.interval_duration;
    }
    else
    {
        pcc_utility_manager->interval_stats_.ack_rate_mbps = 0;
    }
}
void ComputeApproxRttGradientPccUtilityManager(PccUtilityManager *pcc_utility_manager, const MonitorInterval *interval)
{
    xqc_pcc_proteus_t *pcc = pcc_utility_manager->delegate_;
    xqc_connection_t *conn = pcc->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    // Separate all RTT samples in the interval into two halves, and calculate an
    // approximate RTT gradient.
    // 定义两个变量，分别表示第一次和第二次的往返时间
    uint64_t rtt_first_half = 0;
    uint64_t rtt_second_half = 0;
    // 计算间隔中的往返时间样本数量的一半
    size_t num_half_samples = interval->packet_rtt_samples->size / 2;
    // 定义两个变量，分别表示第一次和第二次的往返时间样本数量
    size_t num_first_half_samples = 0;
    size_t num_second_half_samples = 0;
    // 遍历前半部分样本
    for (size_t i = 0; i < num_half_samples; ++i)
    {
        // 如果当前样本可靠，则将其RTT值加到rtt_first_half中，并将num_first_half_samples加1
        PacketRttSample *sample_i = xqc_array_get(interval->packet_rtt_samples, i);
        if (sample_i->is_reliable_for_gradient_calculation)
        {
            rtt_first_half = rtt_first_half + sample_i->sample_rtt;
            num_first_half_samples++;
        }
        // 如果当前样本可靠，则将其RTT值加到rtt_second_half中，并将num_second_half_samples加1
        PacketRttSample *sample_i_num_half_samples = xqc_array_get(interval->packet_rtt_samples, i + num_half_samples);
        if (sample_i_num_half_samples->is_reliable_for_gradient_calculation)
        {
            rtt_second_half = rtt_second_half +
                              sample_i_num_half_samples->sample_rtt;
            num_second_half_samples++;
        }
    }

    // 如果第一半和第二半的样本数量都为0，则将近似RTT梯度设为0，并返回
    if (num_first_half_samples == 0 || num_second_half_samples == 0)
    {
        pcc_utility_manager->interval_stats_.approx_rtt_gradient = 0.0;
        return;
    }
    // 计算第一半和第二半的RTT的平均值
    rtt_first_half =
        rtt_first_half * (1.0 / (float)(num_first_half_samples));
    rtt_second_half =
        rtt_second_half * (1.0 / (float)(num_second_half_samples));
    // 计算近似RTT梯度
    pcc_utility_manager->interval_stats_.approx_rtt_gradient = 2.0 *
                                                               (float)(rtt_second_half - rtt_first_half) /
                                                               (float)(rtt_second_half + rtt_first_half);
}

void ComputeRttGradientPccUtilityManager(PccUtilityManager *pcc_utility_manager, const MonitorInterval *interval)
{
    xqc_pcc_proteus_t *pcc = pcc_utility_manager->delegate_;
    xqc_connection_t *conn = pcc->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    // 如果interval的可靠RTT数量小于2，则将RTT梯度设置为0，RTT梯度截断设置为0，并返回
    if (interval->num_reliable_rtt_for_gradient_calculation < 2)
    {
        pcc_utility_manager->interval_stats_.rtt_gradient = 0;
        pcc_utility_manager->interval_stats_.rtt_gradient_cut = 0;
        return;
    }

    // Calculate RTT gradient using linear regression.
    // 计算梯度
    float gradient_x_avg = 0.0;
    float gradient_y_avg = 0.0;
    float gradient_x = 0.0;
    float gradient_y = 0.0;
    // 遍历每个rtt样本
    for (int i = 0; i < interval->packet_rtt_samples->size; i++)
    {
        const PacketRttSample *rtt_sample = xqc_array_get(interval->packet_rtt_samples, i);
        // 如果样本不可靠，则跳过
        if (!rtt_sample->is_reliable_for_gradient_calculation)
        {
            continue;
        }
        // 计算梯度x的平均值
        gradient_x_avg += (float)(rtt_sample->packet_number);
        // 计算梯度y的平均值
        gradient_y_avg +=
            (float)(rtt_sample->sample_rtt);
    }

    // 计算梯度x的平均值
    gradient_x_avg /=
        (float)(interval->num_reliable_rtt_for_gradient_calculation);
    // 计算梯度y的平均值
    gradient_y_avg /=
        (float)(interval->num_reliable_rtt_for_gradient_calculation);

    for (int i = 0; i < interval->packet_rtt_samples->size; i++)
    {
        const PacketRttSample *rtt_sample = xqc_array_get(interval->packet_rtt_samples, i);
        // 如果该样本不可靠，则跳过
        if (!rtt_sample->is_reliable_for_gradient_calculation)
        {
            continue;
        }
        // 计算样本的包号与平均包号的差值
        float delta_packet_number =
            (float)(rtt_sample->packet_number) - gradient_x_avg;
        // 计算样本的RTT与平均RTT的差值
        float delta_rtt_sample =
            (float)(rtt_sample->sample_rtt) -
            gradient_y_avg;
        // 计算梯度x
        gradient_x += delta_packet_number * delta_packet_number;
        // 计算梯度y
        gradient_y += delta_packet_number * delta_rtt_sample;
    }

    // 计算RTT梯度
    pcc_utility_manager->interval_stats_.rtt_gradient = gradient_y / gradient_x;
    // 将RTT梯度除以最大包大小的传输时间
    pcc_utility_manager->interval_stats_.rtt_gradient /=
        (float)(kMaxPacketSize * 8 * 1000000 / interval->sending_rate);
    // 计算平均RTT
    pcc_utility_manager->interval_stats_.avg_rtt = gradient_y_avg;
    pcc_utility_manager->interval_stats_.rtt_gradient_cut =
        gradient_y_avg - pcc_utility_manager->interval_stats_.rtt_gradient * gradient_x_avg;
}

void ComputeRttGradientErrorPccUtilityManager(PccUtilityManager *pcc_utility_manager, const MonitorInterval *interval)
{
    xqc_pcc_proteus_t *pcc = pcc_utility_manager->delegate_;
    xqc_connection_t *conn = pcc->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    pcc_utility_manager->interval_stats_.rtt_gradient_error = 0.0;
    if (interval->num_reliable_rtt_for_gradient_calculation < 2)
    {
        return;
    }

    for (int i = 0; i < interval->packet_rtt_samples->size; i++)
    {
        const PacketRttSample *rtt_sample = xqc_array_get(interval->packet_rtt_samples, i);
        if (!rtt_sample->is_reliable_for_gradient_calculation)
        {
            continue;
        }
        float regression_rtt = (float)(rtt_sample->packet_number *
                                       pcc_utility_manager->interval_stats_.rtt_gradient) +
                               pcc_utility_manager->interval_stats_.rtt_gradient_cut;
        pcc_utility_manager->interval_stats_.rtt_gradient_error +=
            pow((double)rtt_sample->sample_rtt - regression_rtt, 2.0);
    }

    pcc_utility_manager->interval_stats_.rtt_gradient_error /=
        (float)(interval->num_reliable_rtt_for_gradient_calculation);
    pcc_utility_manager->interval_stats_.rtt_gradient_error = sqrt(pcc_utility_manager->interval_stats_.rtt_gradient_error);
    pcc_utility_manager->interval_stats_.rtt_gradient_error /=
        (float)(pcc_utility_manager->interval_stats_.avg_rtt);
}

void ComputeRttDeviationPccUtilityManager(PccUtilityManager *pcc_utility_manager, const MonitorInterval *interval)
{
    xqc_pcc_proteus_t *pcc = pcc_utility_manager->delegate_;
    xqc_connection_t *conn = pcc->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    if (interval->num_reliable_rtt < 2)
    {
        pcc_utility_manager->interval_stats_.rtt_dev = 0;
        return;
    }

    // Calculate RTT deviation.
    pcc_utility_manager->interval_stats_.rtt_dev = 0.0;
    pcc_utility_manager->interval_stats_.max_rtt = -1;
    pcc_utility_manager->interval_stats_.min_rtt = -1;
    // 遍历interval->packet_rtt_samples中的每个PacketRttSample
    for (int i = 0; i < interval->packet_rtt_samples->size; i++)
    {
        const PacketRttSample *rtt_sample = xqc_array_get(interval->packet_rtt_samples, i);
        // 如果PacketRttSample不可靠，则跳过
        if (!rtt_sample->is_reliable)
        {
            continue;
        }
        // 计算delta_rtt_sample
        float delta_rtt_sample =
            (float)(rtt_sample->sample_rtt) -
            pcc_utility_manager->interval_stats_.avg_rtt;
        // 将delta_rtt_sample的平方加到interval_stats_.rtt_dev中
        pcc_utility_manager->interval_stats_.rtt_dev += delta_rtt_sample * delta_rtt_sample;

        // 如果min_rtt_小于0或者rtt_sample.sample_rtt小于min_rtt_，则更新min_rtt_
        if (pcc_utility_manager->min_rtt_ < 0 || rtt_sample->sample_rtt < pcc_utility_manager->min_rtt_)
        {
            pcc_utility_manager->min_rtt_ = rtt_sample->sample_rtt;
        }
        // 如果interval_stats_.min_rtt小于0或者rtt_sample.sample_rtt小于interval_stats_.min_rtt，则更新interval_stats_.min_rtt
        if (pcc_utility_manager->interval_stats_.min_rtt < 0 ||
            rtt_sample->sample_rtt < pcc_utility_manager->interval_stats_.min_rtt)
        {
            pcc_utility_manager->interval_stats_.min_rtt =
                (float)(rtt_sample->sample_rtt);
        }
        // 如果interval_stats_.max_rtt小于0或者rtt_sample.sample_rtt大于interval_stats_.max_rtt，则更新interval_stats_.max_rtt
        if (pcc_utility_manager->interval_stats_.max_rtt < 0 ||
            rtt_sample->sample_rtt > pcc_utility_manager->interval_stats_.max_rtt)
        {
            pcc_utility_manager->interval_stats_.max_rtt =
                (float)(rtt_sample->sample_rtt);
        }
    }
    pcc_utility_manager->interval_stats_.rtt_dev =
        sqrt(pcc_utility_manager->interval_stats_.rtt_dev /
             (float)(interval->num_reliable_rtt));
}

void ProcessRttTrendPccUtilityManager(PccUtilityManager *pcc_utility_manager, const MonitorInterval *interval)
{
    xqc_pcc_proteus_t *pcc = pcc_utility_manager->delegate_;
    xqc_connection_t *conn = pcc->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    if (interval->num_reliable_rtt < 2)
    {
        return;
    }

    // 将interval_stats_的avg_rtt值添加到mi_avg_rtt_history_中
    PushBackMiAvgRttHistoryQueue(&pcc_utility_manager->mi_avg_rtt_history_, pcc_utility_manager->interval_stats_.avg_rtt);
    // 将interval_stats_的rtt_dev值添加到mi_rtt_dev_history_中
    PushBackMiRttDevHistoryQueue(&pcc_utility_manager->mi_rtt_dev_history_, pcc_utility_manager->interval_stats_.rtt_dev);
    pcc_utility_manager->mi_avg_rtt_history_size_++;
    pcc_utility_manager->mi_rtt_dev_history_size_++;

    if (pcc_utility_manager->mi_avg_rtt_history_size_ > kRttHistoryLen)
    {
        xqc_queue_t *front = xqc_queue_head(&pcc_utility_manager->mi_avg_rtt_history_);
        MiAvgRttHistoryQueue *queue = xqc_queue_data(front, MiAvgRttHistoryQueue, queue);
        xqc_queue_remove(front);
        pcc_utility_manager->mi_avg_rtt_history_size_--;
        xqc_free(queue);
    }
    if (pcc_utility_manager->mi_rtt_dev_history_size_ > kRttHistoryLen)
    {
        xqc_queue_t *front = xqc_queue_head(&pcc_utility_manager->mi_rtt_dev_history_);
        MiRttDevHistoryQueue *queue = xqc_queue_data(front, MiRttDevHistoryQueue, queue);
        pcc_utility_manager->mi_rtt_dev_history_size_--;
        xqc_queue_remove(front);
        xqc_free(queue);
    }

    if (pcc_utility_manager->mi_avg_rtt_history_size_ >= kRttHistoryLen)
    {
        ComputeTrendingGradientPccUtilityManager(pcc_utility_manager);
        ComputeTrendingGradientErrorPccUtilityManager(pcc_utility_manager);

        DetermineToleranceInflationPccUtilityManager(pcc_utility_manager);
    }

    if (pcc_utility_manager->mi_rtt_dev_history_size_ >= kRttHistoryLen)
    {
        ComputeTrendingDeviationPccUtilityManager(pcc_utility_manager);

        DetermineToleranceDeviationPccUtilityManager(pcc_utility_manager);
    }
}

void ComputeTrendingGradientPccUtilityManager(PccUtilityManager *pcc_utility_manager)
{
    xqc_pcc_proteus_t *pcc = pcc_utility_manager->delegate_;
    xqc_connection_t *conn = pcc->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    // Calculate RTT gradient using linear regression.
    float gradient_x_avg = 0.0;
    float gradient_y_avg = 0.0;
    float gradient_x = 0.0;
    float gradient_y = 0.0;
    size_t num_sample = pcc_utility_manager->mi_avg_rtt_history_size_;

    xqc_queue_t *pos;
    size_t i = 0;
    xqc_queue_foreach(pos, &pcc_utility_manager->mi_avg_rtt_history_)
    {
        MiAvgRttHistoryQueue *queue = xqc_queue_data(pos, MiAvgRttHistoryQueue, queue);
        gradient_x_avg += (float)(i++);
        gradient_y_avg += queue->mi_avg_rtt;
    }

    gradient_x_avg /= (float)(num_sample);
    gradient_y_avg /= (float)(num_sample);
    i = 0;
    xqc_queue_foreach(pos, &pcc_utility_manager->mi_avg_rtt_history_)
    {
        MiAvgRttHistoryQueue *queue = xqc_queue_data(pos, MiAvgRttHistoryQueue, queue);
        float delta_x = (float)(i++) - gradient_x_avg;
        float delta_y = queue->mi_avg_rtt - gradient_y_avg;
        gradient_x += delta_x * delta_x;
        gradient_y += delta_x * delta_y;
    }

    pcc_utility_manager->interval_stats_.trending_gradient = gradient_y / gradient_x;
    pcc_utility_manager->interval_stats_.trending_gradient_cut =
        gradient_y_avg - pcc_utility_manager->interval_stats_.trending_gradient * gradient_x_avg;
}

void ComputeTrendingGradientErrorPccUtilityManager(PccUtilityManager *pcc_utility_manager)
{
    xqc_pcc_proteus_t *pcc = pcc_utility_manager->delegate_;
    xqc_connection_t *conn = pcc->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    size_t num_sample = pcc_utility_manager->mi_avg_rtt_history_size_;
    pcc_utility_manager->interval_stats_.trending_gradient_error = 0.0;

    xqc_queue_t *pos;
    size_t i = 0;
    xqc_queue_foreach(pos, &pcc_utility_manager->mi_avg_rtt_history_)
    {
        MiAvgRttHistoryQueue *queue = xqc_queue_data(pos, MiAvgRttHistoryQueue, queue);
        float regression_rtt =
            (float)(i++) * pcc_utility_manager->interval_stats_.trending_gradient +
            pcc_utility_manager->interval_stats_.trending_gradient_cut;
        pcc_utility_manager->interval_stats_.trending_gradient_error +=
            pow(queue->mi_avg_rtt - regression_rtt, 2.0);
    }
    pcc_utility_manager->interval_stats_.trending_gradient_error /= (float)(num_sample);
    pcc_utility_manager->interval_stats_.trending_gradient_error =
        sqrt(pcc_utility_manager->interval_stats_.trending_gradient_error);
}

void ComputeTrendingDeviationPccUtilityManager(PccUtilityManager *pcc_utility_manager)
{
    xqc_pcc_proteus_t *pcc = pcc_utility_manager->delegate_;
    xqc_connection_t *conn = pcc->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    size_t num_sample = pcc_utility_manager->mi_rtt_dev_history_size_;
    float avg_rtt_dev = 0.0;

    xqc_queue_t *pos;

    xqc_queue_foreach(pos, &pcc_utility_manager->mi_avg_rtt_history_)
    {
        MiAvgRttHistoryQueue *queue = xqc_queue_data(pos, MiAvgRttHistoryQueue, queue);
        avg_rtt_dev += queue->mi_avg_rtt;
    }
    avg_rtt_dev /= (float)(num_sample);

    pcc_utility_manager->interval_stats_.trending_deviation = 0.0;

    xqc_queue_foreach(pos, &pcc_utility_manager->mi_avg_rtt_history_)
    {
        MiAvgRttHistoryQueue *queue = xqc_queue_data(pos, MiAvgRttHistoryQueue, queue);
        float delta_dev = avg_rtt_dev - queue->mi_avg_rtt;
        pcc_utility_manager->interval_stats_.trending_deviation += (delta_dev * delta_dev);
    }
    pcc_utility_manager->interval_stats_.trending_deviation /= (float)(num_sample);
    pcc_utility_manager->interval_stats_.trending_deviation = sqrt(pcc_utility_manager->interval_stats_.trending_deviation);
}

void DetermineToleranceGeneralPccUtilityManager(PccUtilityManager *pcc_utility_manager)
{
    xqc_pcc_proteus_t *pcc = pcc_utility_manager->delegate_;
    xqc_connection_t *conn = pcc->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    // 如果interval_stats_的rtt_gradient_error小于interval_stats_的rtt_gradient的绝对值
    if (pcc_utility_manager->interval_stats_.rtt_gradient_error <
        (xqc_sub_abs(pcc_utility_manager->interval_stats_.rtt_gradient, 0.0)))
    {
        // 将is_rtt_inflation_tolerable_设置为false
        pcc_utility_manager->is_rtt_inflation_tolerable_ = XQC_FALSE;
        // 将is_rtt_dev_tolerable_设置为false
        pcc_utility_manager->is_rtt_dev_tolerable_ = XQC_FALSE;
    }
    else
    {
        // 否则将is_rtt_inflation_tolerable_设置为true
        pcc_utility_manager->is_rtt_inflation_tolerable_ = XQC_TRUE;
        // 将is_rtt_dev_tolerable_设置为true
        pcc_utility_manager->is_rtt_dev_tolerable_ = XQC_TRUE;
    }
}
void DetermineToleranceInflationPccUtilityManager(PccUtilityManager *pcc_utility_manager)
{
    xqc_pcc_proteus_t *pcc = pcc_utility_manager->delegate_;
    xqc_connection_t *conn = pcc->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    pcc_utility_manager->ratio_inflated_mi_ *= (1 - kAlpha);
    float trending_gradient_abs = pcc_utility_manager->interval_stats_.trending_gradient > 0 ? pcc_utility_manager->interval_stats_.trending_gradient : -pcc_utility_manager->interval_stats_.trending_gradient;
    if (pcc_utility_manager->min_trending_gradient_ < 0.000001 ||
        trending_gradient_abs < pcc_utility_manager->min_trending_gradient_ / kBeta)
    {
        pcc_utility_manager->avg_trending_gradient_ = 0.0f;
        pcc_utility_manager->min_trending_gradient_ = trending_gradient_abs;
        pcc_utility_manager->dev_trending_gradient_ = trending_gradient_abs;
        pcc_utility_manager->last_trending_gradient_ = pcc_utility_manager->interval_stats_.trending_gradient;
    }
    else
    {
        float dev_gain = pcc_utility_manager->interval_stats_.rtt_dev < 1000
                             ? kInflationToleranceGainLow
                             : kInflationToleranceGainHigh;
        float tolerate_threshold_h =
            pcc_utility_manager->avg_trending_gradient_ + dev_gain * pcc_utility_manager->dev_trending_gradient_;
        float tolerate_threshold_l =
            pcc_utility_manager->avg_trending_gradient_ - dev_gain * pcc_utility_manager->dev_trending_gradient_;
        if (pcc_utility_manager->interval_stats_.trending_gradient < tolerate_threshold_l ||
            pcc_utility_manager->interval_stats_.trending_gradient > tolerate_threshold_h)
        {
            if (pcc_utility_manager->interval_stats_.trending_gradient > 0)
            {
                pcc_utility_manager->is_rtt_inflation_tolerable_ = XQC_FALSE;
            }
            pcc_utility_manager->is_rtt_dev_tolerable_ = XQC_FALSE;
            pcc_utility_manager->ratio_inflated_mi_ += kAlpha;
        }
        else
        {
            float gradient_interval_abs = pcc_utility_manager->interval_stats_.trending_gradient - pcc_utility_manager->last_trending_gradient_;
            gradient_interval_abs = gradient_interval_abs > 0 ? gradient_interval_abs : -gradient_interval_abs;
            pcc_utility_manager->dev_trending_gradient_ =
                pcc_utility_manager->dev_trending_gradient_ * (1 - kAlpha) +
                gradient_interval_abs * kAlpha;
            pcc_utility_manager->avg_trending_gradient_ =
                pcc_utility_manager->avg_trending_gradient_ * (1 - kAlpha) +
                pcc_utility_manager->interval_stats_.trending_gradient * kAlpha;
            pcc_utility_manager->last_trending_gradient_ = pcc_utility_manager->interval_stats_.trending_gradient;
        }

        pcc_utility_manager->min_trending_gradient_ = pcc_utility_manager->min_trending_gradient_ > trending_gradient_abs ? trending_gradient_abs : pcc_utility_manager->min_trending_gradient_;
    }
}
void DetermineToleranceDeviationPccUtilityManager(PccUtilityManager *pcc_utility_manager)
{
    xqc_pcc_proteus_t *pcc = pcc_utility_manager->delegate_;
    xqc_connection_t *conn = pcc->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    pcc_utility_manager->ratio_fluctuated_mi_ *= (1 - kAlpha);

    if (pcc_utility_manager->avg_mi_rtt_dev_ < 0.000001)
    {
        pcc_utility_manager->avg_mi_rtt_dev_ = pcc_utility_manager->interval_stats_.rtt_dev;
        pcc_utility_manager->dev_mi_rtt_dev_ = 0.5 * pcc_utility_manager->interval_stats_.rtt_dev;
    }
    else
    {
        if (pcc_utility_manager->interval_stats_.rtt_dev > pcc_utility_manager->avg_mi_rtt_dev_ + pcc_utility_manager->dev_mi_rtt_dev_ * 4.0 &&
            pcc_utility_manager->interval_stats_.rtt_dev > 1000)
        {
            pcc_utility_manager->is_rtt_dev_tolerable_ = XQC_FALSE;
            pcc_utility_manager->ratio_fluctuated_mi_ += kAlpha;
        }
        else
        {
            float rtt_dev_interval_abs = pcc_utility_manager->interval_stats_.rtt_dev - pcc_utility_manager->avg_mi_rtt_dev_;
            rtt_dev_interval_abs = rtt_dev_interval_abs > 0 ? rtt_dev_interval_abs : -rtt_dev_interval_abs;
            pcc_utility_manager->dev_mi_rtt_dev_ =
                pcc_utility_manager->dev_mi_rtt_dev_ * (1 - kAlpha) +
                rtt_dev_interval_abs * kAlpha;
            pcc_utility_manager->avg_mi_rtt_dev_ =
                pcc_utility_manager->avg_mi_rtt_dev_ * (1 - kAlpha) + pcc_utility_manager->interval_stats_.rtt_dev * kAlpha;
        }
    }

    if (pcc_utility_manager->ratio_fluctuated_mi_ > kTrendingResetIntervalRatio)
    {
        pcc_utility_manager->avg_mi_rtt_dev_ = -1;
        pcc_utility_manager->dev_mi_rtt_dev_ = -1;
        pcc_utility_manager->ratio_fluctuated_mi_ = 0;
    }
}

float CalculateUtilityScavengerPccUtilityManager(PccUtilityManager *pcc_utility_manager, const MonitorInterval *interval,
                                                 float rtt_variance_coefficient)
{
    xqc_pcc_proteus_t *pcc = pcc_utility_manager->delegate_;
    xqc_connection_t *conn = pcc->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    // xi^t
    float sending_rate_contribution =
        pow(pcc_utility_manager->interval_stats_.actual_sending_rate_mbps, kSendingRateExponent);
    // cxiL
    float loss_penalty = kVivaceLossCoefficient * pcc_utility_manager->interval_stats_.loss_rate *
                         pcc_utility_manager->interval_stats_.actual_sending_rate_mbps;
    // max(0,drtt/dt)
    float rtt_gradient =
        pcc_utility_manager->is_rtt_inflation_tolerable_ ? 0.0 : pcc_utility_manager->interval_stats_.rtt_gradient;
    if (interval->rtt_fluctuation_tolerance_ratio > 50.0 &&
        (rtt_gradient > 0 ? rtt_gradient : rtt_gradient) < 1000.0 / pcc_utility_manager->interval_stats_.interval_duration)
    {
        rtt_gradient = 0.0;
    }
    if (rtt_gradient < 0)
    {
        rtt_gradient = 0.0;
    }
    // dxi(rtti)
    float latency_penalty = kLatencyCoefficient * rtt_gradient *
                            pcc_utility_manager->interval_stats_.actual_sending_rate_mbps;

    float rtt_dev = pcc_utility_manager->is_rtt_dev_tolerable_ ? 0.0 : pcc_utility_manager->interval_stats_.rtt_dev;
    if (interval->rtt_fluctuation_tolerance_ratio > 50.0)
    {
        rtt_dev = 0.0;
    }
    // dxi(rtti)
    float rtt_dev_penalty = rtt_variance_coefficient * rtt_dev *
                            pcc_utility_manager->interval_stats_.actual_sending_rate_mbps;

    return sending_rate_contribution - loss_penalty -
           latency_penalty - rtt_dev_penalty;
}

/**
 * based on pcc_sender.cpp and pcc_vivace_sender.cpp
 */
static xqc_bool_t FLAGS_enable_rtt_deviation_based_early_termination = XQC_TRUE;
static xqc_bool_t FLAGS_trigger_early_termination_based_on_interval_queue_front = XQC_FALSE;
static xqc_bool_t FLAGS_enable_early_termination_based_on_latest_rtt_trend = XQC_FALSE;
static double FLAGS_max_rtt_fluctuation_tolerance_ratio_in_starting = 100.0;
static double FLAGS_max_rtt_fluctuation_tolerance_ratio_in_decision_made = 1.0;
static double FLAGS_rtt_fluctuation_tolerance_gain_in_starting = 2.5;
static double FLAGS_rtt_fluctuation_tolerance_gain_in_decision_made = 1.5;
static double FLAGS_rtt_fluctuation_tolerance_gain_in_probing = 1.0;
static xqc_bool_t FLAGS_can_send_respect_congestion_window = XQC_TRUE;
static double FLAGS_bytes_in_flight_gain = 2.5;
static xqc_bool_t FLAGS_exit_starting_based_on_sampled_bandwidth = XQC_FALSE;
static xqc_bool_t FLAGS_restore_central_rate_upon_app_limited = XQC_FALSE;

const size_t kNumIntervalGroupsInProbingScavenger = 2;
const size_t kNumIntervalGroupsInProbingPrimary = 3;

const uint64_t kInitialRtt = 100000;
const uint64_t kDefaultTCPMSS = 1400;
// Number of bits per Mbit.
const size_t kMegabit = 1024 * 1024;
// Minimum number of packets per monitor interval.
const size_t kMinPacketPerInterval = 5;
// Step size for rate change in PROBING mode.
const float kProbingStepSize = 0.05f;
// Base percentile step size for rate change in DECISION_MADE mode.
const float kDecisionMadeStepSize = 0.02f;
// Maximum percentile step size for rate change in DECISION_MADE mode.
const float kMaxDecisionMadeStepSize = 0.10f;
// Bandwidth filter window size in round trips.
const uint64_t kBandwidthWindowSize = 6;
// The factor that converts utility gradient to sending rate change.
float kUtilityGradientToRateChangeFactor = 1.0f;
// The exponent to amplify sending rate change based on number of consecutive
// rounds in DECISION_MADE mode.
float kRateChangeAmplifyExponent = 1.2f;

// The minimum ratio of RTT samples being reliable per MI.
const float kMinReliabilityRatio = 0.8f;

const int initial_congestion_window = 10;
const int max_congestion_window = 10;

const uint64_t kMinRateChange = 500 * 1000;
const uint64_t kMinSendingRate = 500 * 1000;

// The initial maximum rate change step size in Vivace.
float kInitialMaxStepSize = 0.05f;
// The incremental rate change step size allowed on basis of current maximum
// step size every time the calculated rate change exceeds the current max.
float kIncrementalStepSize = 0.05;

const uint64_t kNumMicrosPerSecond = 1000 * 1000;

void OnCongestionEventSender(xqc_pcc_proteus_t *pcc, xqc_bool_t rtt_updated, uint64_t rtt,
                             uint64_t bytes_in_flight,
                             uint64_t event_time,
                             const xqc_array_t *acked_packets, const xqc_array_t *lost_packets)
{
    xqc_connection_t *conn = pcc->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    if (pcc->latest_ack_timestamp_ == 0)
    {
        pcc->latest_ack_timestamp_ = event_time;
    }

    if (pcc->exit_starting_based_on_sampled_bandwidth_)
    {
        // UpdateBandwidthSampler(event_time, acked_packets, lost_packets);
    }

    uint64_t ack_interval = 0;
    if (rtt_updated)
    {
        ack_interval = event_time - pcc->latest_ack_timestamp_;
        UpdateRttSender(pcc, event_time, rtt);
    }
    uint64_t avg_rtt = pcc->avg_rtt_;
    // QUIC_BUG_IF(avg_rtt.IsZero());
    if (!pcc->has_seen_valid_rtt_)
    {
        pcc->has_seen_valid_rtt_ = XQC_TRUE;
        // Update sending rate if the actual RTT is smaller than initial rtt value
        // in RttStats, so PCC can start with larger rate and ramp up faster.
        if (pcc->latest_rtt_ < kInitialRtt)
        {
            pcc->sending_rate_ = pcc->sending_rate_ *
                                 ((float)(kInitialRtt) /
                                  (float)(pcc->latest_rtt_));
        }
    }
    if (pcc->mode_ == STARTING && CheckForRttInflationSender(pcc))
    {
        // Directly enter PROBING when rtt inflation already exceeds the tolerance
        // ratio, so as to reduce packet losses and mitigate rtt inflation.
        OnRttInflationInStartingPccMonitorIntervalQueue(pcc->interval_queue_);
        EnterProbingSender(pcc);
        return;
    }

    OnCongestionEventPccMonitorIntervalQueue(pcc->interval_queue_, acked_packets, lost_packets, avg_rtt,
                                             pcc->latest_rtt_, pcc->min_rtt_, event_time,
                                             ack_interval);
}

void OnPacketSentSender(xqc_pcc_proteus_t *pcc, uint64_t sent_time,
                        uint64_t bytes_in_flight,
                        uint64_t packet_number,
                        uint64_t bytes,
                        xqc_bool_t is_retransmittable)
{
    xqc_connection_t *conn = pcc->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    if (pcc->conn_start_time_ == 0)
    {
        pcc->conn_start_time_ = sent_time;
        pcc->latest_sent_timestamp_ = sent_time;
    }

    // last_sent_packet_ = packet_number;

    // Do not process not retransmittable packets. Otherwise, the interval may
    // never be able to end if one of these packets gets lost.
    if (XQC_FALSE && is_retransmittable != XQC_TRUE)
    {
        return;
    }

    if (CreateNewIntervalSender(pcc, sent_time))
    {
        MaybeSetSendingRateSender(pcc);
        // Set the monitor duration to 1.0 of min rtt.
        pcc->monitor_duration_ = pcc->min_rtt_ * 1.0;

        xqc_bool_t is_useful = CreateUsefulIntervalSender(pcc);
        EnqueueNewMonitorIntervalPccMonitorIntervalQueue(pcc->interval_queue_, is_useful ? pcc->sending_rate_ : GetSendingRateForNonUsefulIntervalSender(pcc),
                                                         is_useful, GetMaxRttFluctuationToleranceSender(pcc), pcc->avg_rtt_);

        /*std::cerr << (sent_time - QuicTime::Zero()).ToMicroseconds() << " "
                  << "Create MI (useful: " << interval_queue_.current().is_useful
                  << ") with rate " << interval_queue_.current().sending_rate
                                                                .ToKBitsPerSecond()
                  << ", duration " << monitor_duration_.ToMicroseconds()
                  << std::endl;*/
    }
    OnPacketSentPccMonitorIntervalQueue(pcc->interval_queue_, sent_time, packet_number, bytes,
                                        sent_time - pcc->latest_sent_timestamp_);

    pcc->latest_sent_timestamp_ = sent_time;
}

xqc_bool_t CanSendSender(xqc_pcc_proteus_t *pcc, uint64_t bytes_in_flight)
{
    xqc_connection_t *conn = pcc->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    if (!FLAGS_can_send_respect_congestion_window)
    {
        return XQC_TRUE;
    }

    if (sizePccMonitorIntervalQueue(pcc->interval_queue_) - num_useful_intervalsPccMonitorIntervalQueue(pcc->interval_queue_) > 4)
    {
        return XQC_FALSE;
    }
    else
    {
        return XQC_TRUE;
    }

    if (pcc->min_rtt_ < pcc->rtt_deviation_)
    {
        // Avoid capping bytes in flight on highly fluctuating path, because that
        // may impact throughput.
        return XQC_TRUE;
    }

    return bytes_in_flight < FLAGS_bytes_in_flight_gain * GetCongestionWindowSender(pcc);
}

uint64_t PacingRateSender(xqc_pcc_proteus_t *pcc, uint64_t bytes_in_flight)
{
    xqc_connection_t *conn = pcc->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    return emptyPccMonitorIntervalQueue(pcc->interval_queue_) ? pcc->sending_rate_
                                                              : currentPccMonitorIntervalQueue(pcc->interval_queue_)->sending_rate;
}

uint64_t GetCongestionWindowSender(xqc_pcc_proteus_t *pcc)
{
    xqc_connection_t *conn = pcc->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    return pcc->sending_rate_ * (pcc->min_rtt_ == 0 ? kInitialRtt : pcc->min_rtt_) / kNumMicrosPerSecond;
}

void OnUtilityAvailableSender(xqc_pcc_proteus_t *pcc,
                              const xqc_array_t *useful_intervals,
                              uint64_t event_time)
{
    xqc_connection_t *conn = pcc->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    // Calculate the utilities for all available intervals.
    xqc_array_t *utility_info = xqc_array_create(xqc_default_allocator, MONITOR_INTERVAL_SAMPLE_CAPACITY, sizeof(UtilityInfo));

    for (size_t i = 0; i < useful_intervals->size; ++i)
    {
        MonitorInterval **monitor_interval_item_ptr = xqc_array_get(useful_intervals, i);
        MonitorInterval *monitor_interval_item = *monitor_interval_item_ptr;
        UtilityInfo utility_info_item;
        utility_info_item.sending_rate = monitor_interval_item->sending_rate;

        utility_info_item.utility = CalculateUtilityPccUtilityManager(pcc->utility_manager_, monitor_interval_item, event_time - pcc->conn_start_time_);


        xqc_array_push_back(utility_info, &utility_info_item);
    }

    UtilityInfo *utility_info_front = xqc_array_get(utility_info, 0);
    switch (pcc->mode_)
    {
    case STARTING:
        assert(utility_info->size == 1u);

        if (utility_info_front->utility > pcc->latest_utility_info_.utility)
        {
            // Stay in STARTING mode. Double the sending rate and update
            // latest_utility.
            pcc->sending_rate_ = pcc->sending_rate_ * 2;
            pcc->latest_utility_info_ = *utility_info_front;
            ++pcc->rounds_;
        }
        else
        {
            // Enter PROBING mode if utility decreases.
            EnterProbingSender(pcc);
        }
        break;
    case PROBING:
        if (CanMakeDecisionSenderVivace(pcc, utility_info))
        {
            if (FLAGS_restore_central_rate_upon_app_limited &&
                currentPccMonitorIntervalQueue(pcc->interval_queue_)->is_useful)
            {
                // If there is no non-useful interval in this round of PROBING, sender
                // needs to change sending_rate_ back to central rate.
                RestoreCentralSendingRateSender(pcc);
            }
            assert(utility_info->size == 2 * GetNumIntervalGroupsInProbingSender(pcc));
            // Enter DECISION_MADE mode if a decision is made.
            SetRateChangeDirectionSenderVivace(pcc, utility_info);
            EnterDecisionMadeSenderVivace(pcc, utility_info);
        }
        else
        {
            EnterProbingSender(pcc);
        }
        if ((pcc->rounds_ > 1 || pcc->mode_ == DECISION_MADE) && pcc->sending_rate_ <= kMinSendingRate)
        {
            pcc->sending_rate_ = kMinSendingRate;
            pcc->incremental_rate_change_step_allowance_ = 0;
            pcc->rounds_ = 1;
            pcc->mode_ = STARTING;
        }
        break;
    case DECISION_MADE:
        assert(utility_info->size == 1u);
        if ((pcc->direction_ == INCREASE &&
             utility_info_front->utility > pcc->latest_utility_info_.utility &&
             utility_info_front->sending_rate > pcc->latest_utility_info_.sending_rate) ||
            (pcc->direction_ == INCREASE &&
             utility_info_front->utility < pcc->latest_utility_info_.utility &&
             utility_info_front->sending_rate < pcc->latest_utility_info_.sending_rate) ||
            (pcc->direction_ == DECREASE &&
             utility_info_front->utility > pcc->latest_utility_info_.utility &&
             utility_info_front->sending_rate < pcc->latest_utility_info_.sending_rate) ||
            (pcc->direction_ == DECREASE &&
             utility_info_front->utility < pcc->latest_utility_info_.utility &&
             utility_info_front->sending_rate > pcc->latest_utility_info_.sending_rate))
        {
            // Remain in DECISION_MADE mode. Keep increasing or decreasing the
            // sending rate.
            EnterDecisionMadeSenderVivace(pcc, utility_info);
            pcc->latest_utility_info_ = *utility_info_front;
        }
        else
        {
            // Enter PROBING mode if utility decreases.
            EnterProbingSenderVivace(pcc, utility_info);
        }
        break;
    }
}

size_t GetNumIntervalGroupsInProbingSender(xqc_pcc_proteus_t *pcc)
{
    xqc_connection_t *conn = pcc->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    return kNumIntervalGroupsInProbingScavenger;
}

void SetUtilityParameterSender(xqc_pcc_proteus_t *pcc, void *param)
{
    xqc_connection_t *conn = pcc->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    SetUtilityParameterPccUtilityManager(pcc->utility_manager_, param);
}

void UpdateRttSender(xqc_pcc_proteus_t *pcc, uint64_t event_time, uint64_t rtt)
{
    xqc_connection_t *conn = pcc->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    pcc->latest_rtt_ = rtt;
    pcc->rtt_deviation_ = pcc->rtt_deviation_ == 0
                              ? rtt / 2
                              : (int64_t)(0.75 * pcc->rtt_deviation_ +
                                          0.25 * xqc_sub_abs(pcc->avg_rtt_, rtt));
    if (pcc->min_rtt_deviation_ == 0 || pcc->rtt_deviation_ < pcc->min_rtt_deviation_)
    {
        pcc->min_rtt_deviation_ = pcc->rtt_deviation_;
    }

    pcc->avg_rtt_ = pcc->avg_rtt_ == 0 ? rtt : pcc->avg_rtt_ * 0.875 + rtt * 0.125;
    if (pcc->min_rtt_ == 0 || rtt < pcc->min_rtt_)
    {
        pcc->min_rtt_ = rtt;
    }
    // std::cerr << (event_time - QuicTime::Zero()).ToMicroseconds() << " New RTT "
    //           << rtt.ToMicroseconds() << std::endl;

    pcc->latest_ack_timestamp_ = event_time;
}

xqc_bool_t CreateNewIntervalSender(xqc_pcc_proteus_t *pcc, uint64_t event_time)
{
    xqc_connection_t *conn = pcc->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    // Start a new monitor interval upon an empty interval queue.
    if (emptyPccMonitorIntervalQueue(pcc->interval_queue_))
    {
        return XQC_TRUE;
    }

    // Do not start new monitor interval before latest RTT is available.
    if (pcc->latest_rtt_ == 0)
    {
        return XQC_FALSE;
    }

    // Start a (useful) interval if latest RTT is available but the queue does not
    // contain useful interval.
    if (num_useful_intervalsPccMonitorIntervalQueue(pcc->interval_queue_) == 0)
    {
        return XQC_TRUE;
    }

    const MonitorInterval *current_interval = currentPccMonitorIntervalQueue(pcc->interval_queue_);
    // Do not start new interval if there is non-useful interval in the tail.
    if (!current_interval->is_useful)
    {
        return XQC_FALSE;
    }

    // Do not start new interval until current useful interval has enough reliable
    // RTT samples, and its duration exceeds the monitor_duration_.
    if (!current_interval->has_enough_reliable_rtt ||
        event_time - current_interval->first_packet_sent_time <
            pcc->monitor_duration_)
    {
        return XQC_FALSE;
    }

    if ((float)(current_interval->num_reliable_rtt) /
            (float)(current_interval->packet_rtt_samples->size) >
        kMinReliabilityRatio)
    {
        // Start a new interval if current useful interval has an RTT reliability
        // ratio larger than kMinReliabilityRatio.
        return XQC_TRUE;
    }
    else if (current_interval->is_monitor_duration_extended)
    {
        // Start a new interval if current useful interval has been extended once.
        return XQC_TRUE;
    }
    else
    {
        // Extend the monitor duration if the current useful interval has not been
        // extended yet, and its RTT reliability ratio is lower than
        // kMinReliabilityRatio.
        pcc->monitor_duration_ = pcc->monitor_duration_ * 2.0;
        extend_current_intervalPccMonitorIntervalQueue(pcc->interval_queue_);
        return XQC_FALSE;
    }
}

xqc_bool_t CreateUsefulIntervalSender(xqc_pcc_proteus_t *pcc)
{
    xqc_connection_t *conn = pcc->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    if (pcc->avg_rtt_ == 0)
    {
        // Create non useful intervals upon starting a connection, until there is
        // valid rtt stats.
        assert(pcc->mode_ == STARTING);
        return XQC_FALSE;
    }
    // In STARTING and DECISION_MADE mode, there should be at most one useful
    // intervals in the queue; while in PROBING mode, there should be at most
    // 2 * GetNumIntervalGroupsInProbing().
    size_t max_num_useful =
        (pcc->mode_ == PROBING) ? 2 * GetNumIntervalGroupsInProbingSender(pcc) : 1;
    return num_useful_intervalsPccMonitorIntervalQueue(pcc->interval_queue_) < max_num_useful;
}

uint64_t GetSendingRateForNonUsefulIntervalSender(xqc_pcc_proteus_t *pcc)
{
    xqc_connection_t *conn = pcc->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    return pcc->sending_rate_;
}

void MaybeSetSendingRateSender(xqc_pcc_proteus_t *pcc)
{
    xqc_connection_t *conn = pcc->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    if (pcc->mode_ != PROBING || (num_available_intervalsPccMonitorIntervalQueue(pcc->interval_queue_) ==
                                      2 * GetNumIntervalGroupsInProbingSender(pcc) &&
                                  !currentPccMonitorIntervalQueue(pcc->interval_queue_)->is_useful))
    {
        // Do not change sending rate when (1) current mode is STARTING or
        // DECISION_MADE (since sending rate is already changed in
        // OnUtilityAvailable), or (2) more than 2 * GetNumIntervalGroupsInProbing()
        // intervals have been created in PROBING mode.
        return;
    }

    if (num_available_intervalsPccMonitorIntervalQueue(pcc->interval_queue_) != 0)
    {
        // Restore central sending rate.
        RestoreCentralSendingRateSender(pcc);

        if (num_available_intervalsPccMonitorIntervalQueue(pcc->interval_queue_) ==
            2 * GetNumIntervalGroupsInProbingSender(pcc))
        {
            // This is the first not useful monitor interval, its sending rate is the
            // central rate.
            return;
        }
    }

    // Sender creates several groups of monitor intervals. Each group comprises an
    // interval with increased sending rate and an interval with decreased sending
    // rate. Which interval goes first is randomly decided.
    if (num_available_intervalsPccMonitorIntervalQueue(pcc->interval_queue_) % 2 == 0)
    {
        pcc->direction_ = (rand() % 2 == 1) ? INCREASE : DECREASE;
    }
    else
    {
        pcc->direction_ = (pcc->direction_ == INCREASE) ? DECREASE : INCREASE;
    }
    if (pcc->direction_ == INCREASE)
    {
        pcc->sending_rate_ = pcc->sending_rate_ * (1 + kProbingStepSize);
    }
    else
    {
        pcc->sending_rate_ = pcc->sending_rate_ * (1 - kProbingStepSize);
    }
}

float GetMaxRttFluctuationToleranceSender(xqc_pcc_proteus_t *pcc)
{
    xqc_connection_t *conn = pcc->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    float tolerance_ratio =
        pcc->mode_ == STARTING
            ? FLAGS_max_rtt_fluctuation_tolerance_ratio_in_starting
            : FLAGS_max_rtt_fluctuation_tolerance_ratio_in_decision_made;
    return tolerance_ratio;

    // if (FLAGS_enable_rtt_deviation_based_early_termination)
    // {
    //     float tolerance_gain = 0.0;
    //     if (mode_ == STARTING)
    //     {
    //         tolerance_gain = FLAGS_rtt_fluctuation_tolerance_gain_in_starting;
    //     }
    //     else if (mode_ == PROBING)
    //     {
    //         tolerance_gain = FLAGS_rtt_fluctuation_tolerance_gain_in_probing;
    //     }
    //     else
    //     {
    //         tolerance_gain = FLAGS_rtt_fluctuation_tolerance_gain_in_decision_made;
    //     }
    //     tolerance_ratio = std::min(
    //         tolerance_ratio,
    //         tolerance_gain *
    //             static_cast<float>(rtt_deviation_.ToMicroseconds()) /
    //             static_cast<float>((avg_rtt_.IsZero() ? kInitialRtt : avg_rtt_)
    //                                    .ToMicroseconds()));
    // }

    // return tolerance_ratio;
}

void RestoreCentralSendingRateSender(xqc_pcc_proteus_t *pcc)
{
    xqc_connection_t *conn = pcc->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    switch (pcc->mode_)
    {
    case STARTING:
        // The sending rate upon exiting STARTING is set separately. This function
        // should not be called while sender is in STARTING mode.
        break;
    case PROBING:
        // Change sending rate back to central probing rate.
        if (currentPccMonitorIntervalQueue(pcc->interval_queue_)->is_useful)
        {
            if (pcc->direction_ == INCREASE)
            {
                pcc->sending_rate_ = pcc->sending_rate_ * (1.0 / (1 + kProbingStepSize));
            }
            else
            {
                pcc->sending_rate_ = pcc->sending_rate_ * (1.0 / (1 - kProbingStepSize));
            }
        }
        break;
    case DECISION_MADE:
        if (pcc->direction_ == INCREASE)
        {
            pcc->sending_rate_ = pcc->sending_rate_ *
                                 (1.0 / (1 + xqc_min(pcc->rounds_ * kDecisionMadeStepSize,
                                                     kMaxDecisionMadeStepSize)));
        }
        else
        {
            pcc->sending_rate_ = pcc->sending_rate_ *
                                 (1.0 / (1 - xqc_min(pcc->rounds_ * kDecisionMadeStepSize,
                                                     kMaxDecisionMadeStepSize)));
        }
        break;
    }
}

xqc_bool_t CanMakeDecisionSenderVivace(xqc_pcc_proteus_t *pcc, const xqc_array_t *utility_info)
{
    xqc_connection_t *conn = pcc->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    if (utility_info->size < 2 * GetNumIntervalGroupsInProbingSender(pcc))
    {
        return XQC_FALSE;
    }

    size_t count_increase = 0;
    size_t count_decrease = 0;
    for (size_t i = 0; i < GetNumIntervalGroupsInProbingSender(pcc); ++i)
    {
        UtilityInfo *utility_info_2i = xqc_array_get(utility_info, 2 * i);
        UtilityInfo *utility_info_2i_plus_1 = xqc_array_get(utility_info, 2 * i + 1);

        xqc_bool_t increase_i =
            utility_info_2i->utility > utility_info_2i_plus_1->utility
                ? utility_info_2i->sending_rate >
                      utility_info_2i_plus_1->sending_rate
                : utility_info_2i->sending_rate <
                      utility_info_2i_plus_1->sending_rate;

        if (increase_i)
        {
            count_increase++;
        }
        else
        {
            count_decrease++;
        }
    }

    return (count_decrease > GetNumIntervalGroupsInProbingSender(pcc) / 2 ||
            count_increase == GetNumIntervalGroupsInProbingSender(pcc));
}

// Set the sending rate to the central rate used in PROBING mode.
void EnterProbingSenderVivace(xqc_pcc_proteus_t *pcc, const xqc_array_t *utility_info)
{
    xqc_connection_t *conn = pcc->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    assert(DECISION_MADE == pcc->mode_);
    pcc->rounds_ = 1;

    uint64_t rate_change = ComputeRateChangeSenderVivace(pcc, utility_info);
    if (pcc->direction_ == INCREASE)
    {
        pcc->sending_rate_ = pcc->sending_rate_ - rate_change;
    }
    else
    {
        pcc->sending_rate_ = pcc->sending_rate_ + rate_change;
    }

    if (pcc->sending_rate_ < kMinSendingRate)
    {
        pcc->sending_rate_ = kMinSendingRate;
        pcc->incremental_rate_change_step_allowance_ = 0;
    }
    pcc->mode_ = PROBING;
}
// Set the sending rate when entering DECISION_MADE from PROBING mode.
void EnterDecisionMadeSenderVivace(xqc_pcc_proteus_t *pcc, const xqc_array_t *utility_info)
{
    xqc_connection_t *conn = pcc->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    if (pcc->mode_ == PROBING)
    {
        pcc->sending_rate_ = pcc->direction_ == INCREASE
                                 ? pcc->sending_rate_ * (1 + kProbingStepSize)
                                 : pcc->sending_rate_ * (1 - kProbingStepSize);
    }

    pcc->rounds_ = pcc->mode_ == PROBING ? 1 : pcc->rounds_ + 1;

    uint64_t rate_change = ComputeRateChangeSenderVivace(pcc, utility_info);
    if (pcc->direction_ == INCREASE)
    {
        pcc->sending_rate_ = pcc->sending_rate_ + rate_change;
    }
    else
    {
        pcc->sending_rate_ = pcc->sending_rate_ - rate_change;
    }

    if (pcc->sending_rate_ < kMinSendingRate)
    {
        pcc->sending_rate_ = kMinSendingRate;
        pcc->mode_ = PROBING;
        pcc->rounds_ = 1;
        pcc->incremental_rate_change_step_allowance_ = 0;
    }
    else
    {
        pcc->mode_ = DECISION_MADE;
    }
}

void EnterProbingSender(xqc_pcc_proteus_t *pcc)
{
    xqc_connection_t *conn = pcc->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    switch (pcc->mode_)
    {
    case STARTING:
        // Fall back to the minimum between halved sending rate and
        // max bandwidth * (1 - 0.05) if there is valid bandwidth sample.
        // Otherwise, simply halve the current sending rate.
        pcc->sending_rate_ = pcc->sending_rate_ * 0.5;
        /*
              if (!BandwidthEstimate().IsZero()) {
                DCHECK(exit_starting_based_on_sampled_bandwidth_);
                sending_rate_ = std::min(sending_rate_,
                                         BandwidthEstimate() * (1 - kProbingStepSize));
              }
        */
        break;
    case DECISION_MADE:
        // FALLTHROUGH_INTENDED;
    case PROBING:
        // Reset sending rate to central rate when sender does not have enough
        // data to send more than 2 * GetNumIntervalGroupsInProbing() intervals.
        RestoreCentralSendingRateSender(pcc);
        break;
    }

    if (pcc->mode_ == PROBING)
    {
        ++pcc->rounds_;
        return;
    }

    pcc->mode_ = PROBING;
    pcc->rounds_ = 1;
}
void EnterDecisionMadeSender(xqc_pcc_proteus_t *pcc)
{
    xqc_connection_t *conn = pcc->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    assert(PROBING == pcc->mode_);

    // Change sending rate from central rate based on the probing rate with higher
    // utility.
    if (pcc->direction_ == INCREASE)
    {
        pcc->sending_rate_ = pcc->sending_rate_ * (1 + kProbingStepSize) *
                             (1 + kDecisionMadeStepSize);
    }
    else
    {
        pcc->sending_rate_ = pcc->sending_rate_ * (1 - kProbingStepSize) *
                             (1 - kDecisionMadeStepSize);
    }

    pcc->mode_ = DECISION_MADE;
    pcc->rounds_ = 1;
}
uint64_t ComputeRateChangeSenderVivace(xqc_pcc_proteus_t *pcc, const xqc_array_t *utility_info)
{
    xqc_connection_t *conn = pcc->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    assert(pcc->mode_ != STARTING);

    // Compute rate difference between higher and lower sending rate, as well as
    // their utility difference.
    uint64_t delta_sending_rate = 0;
    float delta_utility = 0.0;
    if (pcc->mode_ == PROBING)
    {
        UtilityInfo *utility_info_0 = xqc_array_get(utility_info, 0);
        UtilityInfo *utility_info_1 = xqc_array_get(utility_info, 1);

        delta_sending_rate =
            xqc_max(utility_info_0->sending_rate, utility_info_1->sending_rate) -
            xqc_min(utility_info_0->sending_rate, utility_info_1->sending_rate);

        for (size_t i = 0; i < GetNumIntervalGroupsInProbingSender(pcc); ++i)
        {
            UtilityInfo *utility_info_2i = xqc_array_get(utility_info, 2 * i);
            UtilityInfo *utility_info_2i_plus_1 = xqc_array_get(utility_info, 2 * i + 1);
            xqc_bool_t increase_i =
                utility_info_2i->utility > utility_info_2i_plus_1->utility
                    ? utility_info_2i->sending_rate >
                          utility_info_2i_plus_1->sending_rate
                    : utility_info_2i->sending_rate <
                          utility_info_2i_plus_1->sending_rate;
            if ((increase_i && pcc->direction_ == DECREASE) ||
                (!increase_i && pcc->direction_ == INCREASE))
            {
                continue;
            }

            delta_utility = delta_utility +
                            xqc_max(utility_info_2i->utility,
                                    utility_info_2i_plus_1->utility) -
                            xqc_min(utility_info_2i->utility,
                                    utility_info_2i_plus_1->utility);
        }
        delta_utility /= (float)(GetNumIntervalGroupsInProbingSender(pcc));
    }
    else
    {
        UtilityInfo *utility_info_0 = xqc_array_get(utility_info, 0);
        delta_sending_rate =
            xqc_max(utility_info_0->sending_rate,
                    pcc->latest_utility_info_.sending_rate) -
            xqc_min(utility_info_0->sending_rate,
                    pcc->latest_utility_info_.sending_rate);
        delta_utility =
            xqc_max(utility_info_0->utility, pcc->latest_utility_info_.utility) -
            xqc_min(utility_info_0->utility, pcc->latest_utility_info_.utility);
    }

    assert(delta_sending_rate != 0);

    float utility_gradient =
        kMegabit * delta_utility / delta_sending_rate;
    uint64_t rate_change =
        utility_gradient * kMegabit * kUtilityGradientToRateChangeFactor;
    if (pcc->mode_ == DECISION_MADE)
    {
        // Amplify rate change amount when sending rate changes towards the same
        // direction more than once.
        rate_change = rate_change * pow((float)((pcc->rounds_ + 1) / 2),
                                        kRateChangeAmplifyExponent);
    }
    else
    {
        // Reset allowed incremental rate change step size upon entering PROBING.
        pcc->incremental_rate_change_step_allowance_ = 0;
    }

    uint64_t max_allowed_rate_change =
        pcc->sending_rate_ * (kInitialMaxStepSize +
                              kIncrementalStepSize * (float)(pcc->incremental_rate_change_step_allowance_));
    if (rate_change > max_allowed_rate_change)
    {
        rate_change = max_allowed_rate_change;
        // Increase incremental rate change step size if the calculated rate change
        // exceeds the current maximum.
        pcc->incremental_rate_change_step_allowance_++;
    }
    else if (pcc->incremental_rate_change_step_allowance_ > 0)
    {
        // Reduce incremental rate change allowance if calculated rate is smaller
        // than the current maximum.
        pcc->incremental_rate_change_step_allowance_--;
    }

    return xqc_max(rate_change, kMinRateChange);
}
void SetRateChangeDirectionSenderVivace(xqc_pcc_proteus_t *pcc, const xqc_array_t *utility_info)
{
    xqc_connection_t *conn = pcc->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    size_t count_increase = 0;
    size_t count_decrease = 0;
    for (size_t i = 0; i < GetNumIntervalGroupsInProbingSender(pcc); ++i)
    {
        UtilityInfo *utility_info_2i = xqc_array_get(utility_info, 2 * i);
        UtilityInfo *utility_info_2i_plus_1 = xqc_array_get(utility_info, 2 * i + 1);
        xqc_bool_t increase_i =
            utility_info_2i->utility > utility_info_2i_plus_1->utility
                ? utility_info_2i->sending_rate >
                      utility_info_2i_plus_1->sending_rate
                : utility_info_2i->sending_rate <
                      utility_info_2i_plus_1->sending_rate;

        if (increase_i)
        {
            count_increase++;
        }
        else
        {
            count_decrease++;
        }
    }

    pcc->direction_ = count_increase > count_decrease ? INCREASE : DECREASE;

    // Store latest utility in the meanwhile.
    for (size_t i = 0; i < GetNumIntervalGroupsInProbingSender(pcc); ++i)
    {
        UtilityInfo *utility_info_2i = xqc_array_get(utility_info, 2 * i);
        UtilityInfo *utility_info_2i_plus_1 = xqc_array_get(utility_info, 2 * i + 1);
        xqc_bool_t increase_i =
            utility_info_2i->utility > utility_info_2i_plus_1->utility
                ? utility_info_2i->sending_rate >
                      utility_info_2i_plus_1->sending_rate
                : utility_info_2i->sending_rate <
                      utility_info_2i_plus_1->sending_rate;

        if ((increase_i && pcc->direction_ == INCREASE) ||
            (!increase_i && pcc->direction_ == DECREASE))
        {
            pcc->latest_utility_info_ =
                utility_info_2i->utility > utility_info_2i_plus_1->utility
                    ? *utility_info_2i
                    : *utility_info_2i_plus_1;
        }
    }
}

xqc_bool_t CheckForRttInflationSender(xqc_pcc_proteus_t *pcc)
{
    xqc_connection_t *conn = pcc->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    if (emptyPccMonitorIntervalQueue(pcc->interval_queue_) ||
        frontPccMonitorIntervalQueue(pcc->interval_queue_)->rtt_on_monitor_start == 0 ||
        pcc->latest_rtt_ <= pcc->avg_rtt_)
    {
        // RTT is not inflated if latest RTT is no larger than smoothed RTT.
        pcc->rtt_on_inflation_start_ = 0;
        return XQC_FALSE;
    }

    // Once the latest RTT exceeds the smoothed RTT, store the corresponding
    // smoothed RTT as the RTT at the start of inflation. RTT inflation will
    // continue as long as latest RTT keeps being larger than smoothed RTT.
    if (pcc->rtt_on_inflation_start_ == 0)
    {
        pcc->rtt_on_inflation_start_ = pcc->avg_rtt_;
    }

    const float max_inflation_ratio = 1 + GetMaxRttFluctuationToleranceSender(pcc);
    const uint64_t rtt_on_monitor_start =
        FLAGS_trigger_early_termination_based_on_interval_queue_front
            ? frontPccMonitorIntervalQueue(pcc->interval_queue_)->rtt_on_monitor_start
            : frontPccMonitorIntervalQueue(pcc->interval_queue_)->rtt_on_monitor_start;
    xqc_bool_t is_inflated =
        max_inflation_ratio * rtt_on_monitor_start < pcc->avg_rtt_;
    if (!is_inflated &&
        FLAGS_enable_early_termination_based_on_latest_rtt_trend)
    {
        // If enabled, check if early termination should be triggered according to
        // the stored smoothed rtt on inflation start.
        is_inflated = max_inflation_ratio * pcc->rtt_on_inflation_start_ <
                      pcc->avg_rtt_;
    }
    if (is_inflated)
    {
        // RTT is inflated by more than the tolerance, and early termination will be
        // triggered. Reset the rtt on inflation start.
        pcc->rtt_on_inflation_start_ = 0.;
    }
    return is_inflated;
}

size_t
xqc_pcc_proteus_size()
{
    return sizeof(xqc_pcc_proteus_t);
}

static void
xqc_pcc_proteus_init(void *cong_ctl, xqc_send_ctl_t *ctl_ctx, xqc_cc_params_t cc_params)
{
    xqc_pcc_proteus_t *pcc = (xqc_pcc_proteus_t *)cong_ctl;

    pcc->send_ctl = ctl_ctx;
    pcc->mode_ = STARTING;
    pcc->sending_rate_ = (initial_congestion_window * kDefaultTCPMSS * kNumMicrosPerSecond) / kInitialRtt * 8;
    pcc->has_seen_valid_rtt_ = XQC_FALSE;
    pcc->latest_utility_ = 0.0;
    pcc->conn_start_time_ = 0;
    pcc->monitor_duration_ = 0;
    pcc->direction_ = INCREASE;
    pcc->rounds_ = 1;
    pcc->interval_queue_ = xqc_pcc_monitor_interval_queue_create(pcc);
    pcc->rtt_on_inflation_start_ = 0;
    pcc->max_cwnd_bytes_ = max_congestion_window * kDefaultTCPMSS;
    pcc->rtt_deviation_ = 0;
    pcc->min_rtt_deviation_ = 0;
    pcc->latest_rtt_ = 0;
    pcc->min_rtt_ = 0;
    pcc->avg_rtt_ = 0;
    pcc->exit_starting_based_on_sampled_bandwidth_ = FLAGS_exit_starting_based_on_sampled_bandwidth;
    pcc->latest_sent_timestamp_ = 0;
    pcc->latest_ack_timestamp_ = 0;
    pcc->latest_utility_info_ = (UtilityInfo){0, 0.0};
    pcc->incremental_rate_change_step_allowance_ = 0;
    pcc->utility_manager_ = xqc_pcc_utility_manager_create(pcc);

    SetUtilityParameterSender(pcc, (float *)&cc_params.scavenger_param);
}

static void
xqc_pcc_proteus_destroy(void *cong_ctl)
{
    xqc_pcc_proteus_t *pcc_proteus = (xqc_pcc_proteus_t *)cong_ctl;
    xqc_connection_t *conn = pcc_proteus->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    xqc_pcc_monitor_interval_queue_destroy(pcc_proteus->interval_queue_);
    xqc_pcc_utility_manager_destroy(pcc_proteus->utility_manager_);
}

static void
xqc_pcc_proteus_on_ack(void *cong_ctl, xqc_packet_out_t *po, xqc_usec_t now, uint64_t rtt)
{
    xqc_pcc_proteus_t *pcc_proteus = (xqc_pcc_proteus_t *)cong_ctl;
    xqc_connection_t *conn = pcc_proteus->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    xqc_array_t *acked_packets = xqc_array_create(xqc_default_allocator, MONITOR_INTERVAL_SAMPLE_CAPACITY, sizeof(AckedPacket));
    xqc_array_t *lost_packets = xqc_array_create(xqc_default_allocator, MONITOR_INTERVAL_SAMPLE_CAPACITY, sizeof(LostPacket));
    AckedPacket acked_packet = {po->po_pkt.pkt_num, po->po_used_size, now};
    xqc_array_push_back(acked_packets, &acked_packet);
    OnCongestionEventSender(pcc_proteus, XQC_TRUE, rtt, 0, now, acked_packets, lost_packets);
    xqc_array_destroy(acked_packets);
    xqc_array_destroy(lost_packets);
}

static int32_t
xqc_pcc_proteus_in_slow_start(void *cong_ctl)
{
    xqc_pcc_proteus_t *pcc_proteus = (xqc_pcc_proteus_t *)cong_ctl;
    xqc_connection_t *conn = pcc_proteus->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    return pcc_proteus->mode_ == STARTING;
}

static void
xqc_pcc_proteus_on_lost(void *cong_ctl, xqc_packet_out_t *po, xqc_usec_t now)
{
    xqc_pcc_proteus_t *pcc_proteus = (xqc_pcc_proteus_t *)cong_ctl;
    xqc_connection_t *conn = pcc_proteus->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    xqc_array_t *acked_packets = xqc_array_create(xqc_default_allocator, MONITOR_INTERVAL_SAMPLE_CAPACITY, sizeof(AckedPacket));
    xqc_array_t *lost_packets = xqc_array_create(xqc_default_allocator, MONITOR_INTERVAL_SAMPLE_CAPACITY, sizeof(LostPacket));
    LostPacket lost_packet = {po->po_pkt.pkt_num, po->po_used_size};
    xqc_array_push_back(lost_packets, &lost_packet);
    OnCongestionEventSender(pcc_proteus, XQC_FALSE, 0, 0, now, acked_packets, lost_packets);
    xqc_array_destroy(acked_packets);
    xqc_array_destroy(lost_packets);
}

static uint64_t
xqc_pcc_proteus_get_cwnd(void *cong_ctl)
{
    xqc_pcc_proteus_t *pcc_proteus = (xqc_pcc_proteus_t *)cong_ctl;
    xqc_connection_t *conn = pcc_proteus->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    return GetCongestionWindowSender(pcc_proteus);
}

static uint32_t
xqc_pcc_proteus_get_pacing_rate(void *cong_ctl)
{
    xqc_pcc_proteus_t *pcc_proteus = (xqc_pcc_proteus_t *)cong_ctl;
    xqc_connection_t *conn = pcc_proteus->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    return PacingRateSender(pcc_proteus, 0);
}

static void
xqc_pcc_proteus_restart_from_idle(void *cong_ctl, uint64_t arg)
{
    return;
}

static int
xqc_pcc_proteus_in_recovery(void *cong_ctl)
{
    return 0;
}



static void
xqc_pcc_proteus_print_status(void *cong_ctl, xqc_sample_t *sampler)
{
    xqc_pcc_proteus_t *pcc_proteus = (xqc_pcc_proteus_t *)cong_ctl;
    xqc_send_ctl_t *send_ctl = sampler->send_ctl;
    xqc_connection_t *conn = send_ctl->ctl_conn;

    const char *mode[] = {"STARTING", "PROBING", "DECISION_MADE"};
    const char *direction[] = {"INCREASE", "DECREASE"};
    xqc_extra_log(conn->log, conn->CS_extra_log, "[CCA:PCC] [status:%s] [direction:%s]",
                  mode[pcc_proteus->mode_], direction[pcc_proteus->direction_]);
    xqc_log(conn->log, XQC_LOG_INFO, "[CCA:PCC] [status:%s] [direction:%s]",
            mode[pcc_proteus->mode_], direction[pcc_proteus->direction_]);
}

static void
xqc_pcc_proteus_set_pacing_rate_CS(void *cong_ctl, uint32_t pacing_rate)
{
    xqc_pcc_proteus_t *pcc_proteus = (xqc_pcc_proteus_t *)cong_ctl;
    pcc_proteus->sending_rate_ = pacing_rate;
}

static void
xqc_pcc_proteus_on_packet_sent(void *cong_ctl, xqc_packet_out_t *po, xqc_usec_t now)
{
    xqc_pcc_proteus_t *pcc_proteus = (xqc_pcc_proteus_t *)cong_ctl;
    xqc_connection_t *conn = pcc_proteus->send_ctl->ctl_conn;
    // xqc_extra_log(conn->log, conn->CS_extra_log, "[here]");
    OnPacketSentSender(pcc_proteus, now, 0, po->po_pkt.pkt_num, po->po_used_size, XQC_FALSE);
}

static void
xqc_pcc_proteus_set_rtt(void *cong_ctl, uint64_t min_rtt, uint64_t avg_rtt)
{
    xqc_pcc_proteus_t *pcc_proteus = (xqc_pcc_proteus_t *)cong_ctl;
    pcc_proteus->min_rtt_ = min_rtt;
    pcc_proteus->avg_rtt_ = avg_rtt;
}

xqc_cong_ctrl_callback_t xqc_pcc_proteus_cb = {
    .xqc_cong_ctl_size = xqc_pcc_proteus_size,
    .xqc_cong_ctl_init = xqc_pcc_proteus_init,
    .xqc_cong_ctl_on_ack_pcc = xqc_pcc_proteus_on_ack,
    .xqc_cong_ctl_in_slow_start = xqc_pcc_proteus_in_slow_start,
    .xqc_cong_ctl_on_lost_pcc = xqc_pcc_proteus_on_lost,
    .xqc_cong_ctl_get_cwnd = xqc_pcc_proteus_get_cwnd,
    .xqc_cong_ctl_get_pacing_rate = xqc_pcc_proteus_get_pacing_rate,
    .xqc_cong_ctl_restart_from_idle = xqc_pcc_proteus_restart_from_idle,
    .xqc_cong_ctl_in_recovery = xqc_pcc_proteus_in_recovery,
    .xqc_cong_ctl_print_status = xqc_pcc_proteus_print_status,
    .xqc_cong_ctl_set_pacing_rate = xqc_pcc_proteus_set_pacing_rate_CS,
    .xqc_cong_ctl_on_packet_sent = xqc_pcc_proteus_on_packet_sent,
    .xqc_cong_ctl_destroy = xqc_pcc_proteus_destroy,
    .xqc_cong_ctl_set_rtt = xqc_pcc_proteus_set_rtt,
};