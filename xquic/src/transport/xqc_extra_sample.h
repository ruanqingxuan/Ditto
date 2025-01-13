// added by jndu

#ifndef _XQC_CALC_H_INCLUDED
#define _XQC_CALC_H_INCLUDED


#include <src/transport/xqc_send_ctl.h>
#include <src/transport/xqc_packet_out.h>
#include <src/transport/xqc_conn.h>
#include <src/transport/xqc_recv_record.h>


typedef struct
{
    xqc_pktno_range_t send_pktno_range;
    int num_of_drop;
}xqc_calc_loss_list_t;

typedef struct
{
    xqc_calc_loss_list_t drop_range;
    xqc_list_head_t list_head;
}xqc_calc_loss_list_node_t;


static inline uint64_t xqc_extra_sample_pkt_stream_size(xqc_packet_out_t* packet_out)
{
    xqc_po_stream_frame_t* stream_frame = packet_out->po_stream_frames;
    uint64_t size = 0;
    for (int i = 0; i < XQC_MAX_STREAM_FRAME_IN_PO; i++)
        size += stream_frame[i].ps_length;
    return size;
}

static inline float xqc_extra_sample_throughput(uint64_t size, xqc_usec_t time)
{
    /* 单位MB/s */
    if (time <= 0) {
        return 0;
    }
    float f_acked_size = size * 1000000;
    float f_time = time;
    return f_acked_size / f_time / 1024.0 / 1024.0;
}

void xqc_extra_sample_loss_list_init(xqc_calc_loss_list_node_t* drop);

void xqc_extra_sample_loss_list_add(xqc_calc_loss_list_node_t* drop, xqc_packet_number_t high, xqc_packet_number_t low);

void xqc_extra_sample_loss_list_del(xqc_calc_loss_list_node_t* node, xqc_list_head_t *del_pos);

void xqc_extra_sample_loss_list_update(xqc_send_ctl_t* send_ctl, xqc_packet_number_t lost, xqc_bool_t flag);

#endif
