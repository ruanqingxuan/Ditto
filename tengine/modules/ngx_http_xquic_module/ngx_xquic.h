/*
 * Copyright (C) 2020-2023 Alibaba Group Holding Limited
 */

#ifndef _T_NGX_XQUIC_H_INCLUDED_
#define _T_NGX_XQUIC_H_INCLUDED_


#include <ngx_core.h>
#include <ngx_config.h>
#include <ngx_http.h>
#include <ngx_http_v3_stream.h>
#include <ngx_xquic_send.h>

#include <xquic/xquic_typedef.h>
#include <xquic/xquic.h>

/* added by jndu */
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <dlfcn.h>


#define NGX_HTTP_V3_INT_OCTETS           4
#define NGX_HTTP_V3_MAX_FIELD                                                 \
    (127 + (1 << (NGX_HTTP_V3_INT_OCTETS - 1) * 7) - 1)

#define NGX_XQUIC_SUPPORT_CID_ROUTE 1

/* added by jndu */
#define NGX_XQUIC_CS_METRIC_CWND_UP "cs_metric_cwnd_up"
#define NGX_XQUIC_CS_METRIC_CWND_DOWN "cs_metric_cwnd_down"
#define NGX_XQUIC_CS_METRIC_IP_INFO "cs_metric_ip_info"
#define NGX_XQUIC_CS_METRIC_SUM_UP "cs_metric_sum_up"
#define NGX_XQUIC_CS_METRIC_FREE_RSC "cs_metric_free_resource"
#define NGX_XQUIC_CS_METRIC_CWND_UP_THRES "cs_metric_cwnd_up_thres"
#define NGX_XQUIC_CS_METRIC_CWND_DOWN_THRES "cs_metric_cwnd_down_thres"
#define NGX_XQUIC_CS_METRIC_NOTICE_CAN_EVALUATE "cs_metric_notice_can_evaluate"

int ngx_xquic_conn_accept(xqc_engine_t *engine, xqc_connection_t *conn, 
    const xqc_cid_t * cid, void * user_data);
void ngx_xquic_conn_refuse(xqc_engine_t *engine, xqc_connection_t *conn, 
    const xqc_cid_t *cid, void *user_data);

int ngx_http_v3_conn_create_notify(xqc_h3_conn_t *h3_conn, const xqc_cid_t *cid, void *user_data);
int ngx_http_v3_conn_close_notify(xqc_h3_conn_t *h3_conn, const xqc_cid_t *cid, void *user_data);
void ngx_http_v3_conn_handshake_finished(xqc_h3_conn_t *h3_conn, void *user_data);
void ngx_http_v3_conn_update_cid_notify(xqc_connection_t *conn, const xqc_cid_t *retire_cid,
    const xqc_cid_t *new_cid, void *conn_user_data);

void ngx_xquic_engine_set_event_timer(xqc_msec_t wake_after, void *user_data);

void ngx_xquic_log_write_err(xqc_log_level_t lvl, const void *buf, size_t size, void *engine_user_data);
void ngx_xquic_log_write_stat(xqc_log_level_t lvl, const void *buf, size_t size, void *engine_user_data);

ngx_int_t ngx_xquic_process_init(ngx_cycle_t *cycle);
void ngx_xquic_process_exit(ngx_cycle_t *cycle);
ngx_int_t ngx_xquic_engine_init(ngx_cycle_t *cycle);

uint64_t ngx_xquic_get_time();

/* added by jndu for CCA switching */
char * xqc_local_addr2str(const struct sockaddr *local_addr, socklen_t local_addrlen);
char * xqc_peer_addr2str(const struct sockaddr *peer_addr, socklen_t peer_addrlen);
void * xqc_server_get_mp_by_addr(xqc_CCA_info_container_t *container, const struct sockaddr *sa_peer, socklen_t peer_addrlen);
xqc_int_t xqc_server_load_CCA_info(xmlDocPtr doc, xmlNodePtr cur, xqc_ip_CCA_info_t *CCA_info);
xqc_int_t xqc_server_load_CCA_param(xqc_CCA_info_container_t *container, const char* file);
xqc_int_t xqc_server_save_CCA_info(xmlNodePtr ip_node, xqc_ip_CCA_info_t **mp_map);
xqc_int_t xqc_server_save_CCA_param(xqc_CCA_info_container_t *container, const char* file);
float xqc_CCA_switching_get_metric(xqc_stream_CCA_info_t *CCA_sampler, xqc_ip_CCA_info_t *CCA_info, int index);


#if (NGX_XQUIC_SUPPORT_CID_ROUTE)

ssize_t
ngx_xquic_cid_generate_cb(const xqc_cid_t *ori_cid, uint8_t *cid_buf, size_t cid_buflen, void *engine_user_data);

/* worker ID is 4 bytes */
#define NGX_QUIC_CID_ROUTE_WORKER_ID_LENGTH         (4)

/**
 * @return CID length based on negotiation result
 * */
uint32_t    ngx_xquic_cid_length(ngx_cycle_t *cycle);

/**
 * enable CID router by other mod 
 * @return  NGX_OK for success, other for failed
 * */
ngx_int_t   ngx_xquic_enable_cid_route(ngx_cycle_t *cycle);

/**
 * return 1 on cid route on, other for off
 * */
ngx_int_t   ngx_xquic_is_cid_route_on(ngx_cycle_t *cycle);

/**
 * offset of worker ID in the CID
 * */
uint32_t    ngx_xquic_cid_worker_id_offset(ngx_cycle_t *cycle);

/**
 * process secret key of the worker ID
 * */
uint32_t    ngx_xquic_cid_worker_id_secret(ngx_cycle_t *cycle);

/**
 * process salt range of the worker ID
 * */
uint32_t    ngx_xquic_cid_worker_id_salt_range(ngx_cycle_t *cycle);

#endif

#endif /* _T_NGX_XQUIC_H_INCLUDED_ */

