/*
 * Copyright (C) 2020-2023 Alibaba Group Holding Limited
 */

#include <ngx_xquic_send.h>
#include <ngx_http_xquic_module.h>
#include <ngx_http_v3_stream.h>
#include <xquic/xquic.h>

#if (T_NGX_HAVE_XUDP)
#include <ngx_xudp.h>
#endif

#define NGX_XQUIC_MAX_SEND_MSG_ONCE  XQC_MAX_SEND_MSG_ONCE

static ssize_t ngx_http_xquic_on_write_block(ngx_http_xquic_connection_t *qc, ngx_event_t *wev);

void
ngx_http_xquic_write_handler(ngx_event_t *wev) // 用于处理写事件的回调
{
    ngx_int_t                     rc;
    ngx_connection_t             *c;
    ngx_http_xquic_connection_t  *qc;

    c = wev->data;
    qc = c->data;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "|xquic|ngx_http_xquic_write_handler|");

    // del write event 删除写事件
    ngx_del_event(wev, NGX_WRITE_EVENT, 0);

    // 如果写事件超时
    if (wev->timedout) {
        ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0,
                       "http3 write event timed out");
        c->error = 1;
        ngx_http_v3_connection_error(qc, NGX_XQUIC_CONN_WRITE_ERR, "write event timed out");
        return;
    }

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0, "xquic write handler");

    // 设置连接为阻塞状态
    qc->blocked = 1;

    // 调用 xqc_conn_continue_send 函数继续发送数据
    rc = xqc_conn_continue_send(qc->engine, &qc->dcid);

    // 如果发送出错
    if (rc < 0) {

        ngx_log_error(NGX_LOG_WARN, c->log, 0, "|xquic|write handler continue send|rc=%i|", rc);

        c->error = 1;
        ngx_http_v3_connection_error(qc, NGX_XQUIC_CONN_WRITE_ERR, 
                                    "xqc_conn_continue_send err");
        return;
    }

    // 取消连接的阻塞状态
    qc->blocked = 0;

    //ngx_http_v3_handle_connection(qc);
}

ssize_t 
ngx_xquic_server_send(const unsigned char *buf, size_t size,
    const struct sockaddr *peer_addr, socklen_t peer_addrlen, void *user_data) // 向 QUIC 客户端发送数据
{

    ngx_log_error(NGX_LOG_DEBUG, ngx_cycle->log, 0,
                                    "|xquic|ngx_xquic_server_send|%p|%z|", buf, size);
    
    //wqntest
    printf("|wqntest|ngx_xquic_server_send\n");
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "|wqntest|ngx_xquic_server_send\n");

    /* while sending reset, user_data may be empty */
    // 检查用户数据是否为空
    ngx_http_xquic_connection_t *qc = (ngx_http_xquic_connection_t *)user_data; 
    if (qc == NULL) {
        ngx_log_error(NGX_LOG_WARN, ngx_cycle->log, 0,
                                    "|xquic|ngx_xquic_server_send|user_conn=NULL|");
        return XQC_SOCKET_ERROR;
    }

    // 初始化结果和文件描述符
    ssize_t res = 0;
    ngx_socket_t fd = qc->connection->fd;
    ngx_log_error(NGX_LOG_DEBUG, ngx_cycle->log, 0,
                    "|xquic|xqc_server_send size=%z now=%i|dcid=%s|", 
                    size, ngx_xquic_get_time(), xqc_dcid_str(&qc->dcid));
    // 循环发送数据
    do {
        errno = 0;
        res = sendto(fd, buf, size, 0, peer_addr, peer_addrlen);
        ngx_log_error(NGX_LOG_DEBUG, ngx_cycle->log, 0,
                        "|xquic|xqc_server_send write %zd, %s|", res, strerror(errno));

        // 如果发送阻塞，则跳出循环
        if ((res < 0) && (errno == EAGAIN)) {
            break;
        }

    } while ((res < 0) && (errno == EINTR));

    // 处理发送阻塞的情况
    if ((res < 0) && (errno == EAGAIN)) {
        return ngx_http_xquic_on_write_block(qc, qc->connection->write);
    } else if (res < 0) {
        // 处理其他发送错误情况
        ngx_log_error(NGX_LOG_WARN, ngx_cycle->log, 0,
                    "|xquic|ngx_xquic_server_send|socket err|");          
        return XQC_SOCKET_ERROR;
    }

    return res;
}


#if defined(T_NGX_XQUIC_SUPPORT_SENDMMSG)
ssize_t 
ngx_xquic_server_send_mmsg(const struct iovec *msg_iov, unsigned int vlen,
    const struct sockaddr *peer_addr, socklen_t peer_addrlen, void *user_data) //发送数据
{
    ngx_event_t               *wev;
    ssize_t                    res = 0;
    unsigned int               i = 0;

    // 定义 sendmmsg 使用的消息头数组
    struct mmsghdr             msg[NGX_XQUIC_MAX_SEND_MSG_ONCE];

    // 初始化消息头数组
    memset(msg, 0, sizeof(msg));

     // 获取连接上下文
    ngx_http_xquic_connection_t *qc = (ngx_http_xquic_connection_t *)user_data;

    // 检查连接上下文是否为空
    if (qc == NULL) {
        ngx_log_error(NGX_LOG_WARN, ngx_cycle->log, 0,
                                    "|xquic|ngx_xquic_server_send_mmsg|user_conn=NULL|");
        return (ssize_t)NGX_ERROR;
    }

    // 获取文件描述符和写事件
    ngx_socket_t fd = qc->connection->fd;
    ngx_log_error(NGX_LOG_DEBUG, ngx_cycle->log, 0,
                    "|xquic|ngx_xquic_server_send_mmsg|vlen=%z now=%i|dcid=%s|",
                    vlen, ngx_xquic_get_time(), xqc_dcid_str(&qc->dcid));

    wev = qc->connection->write;

#if (T_NGX_UDPV2)
#if (T_NGX_HAVE_XUDP)
    if (ngx_xudp_is_tx_enable(qc->connection)) {
        res = ngx_xudp_sendmmsg(qc->connection, (struct iovec *) msg_iov, vlen, peer_addr, peer_addrlen, /**push*/ 1);
        if (res == vlen) {
            return res;
        }else if(res < vlen) {
            if (res < 0) {
                if (ngx_xudp_error_is_fatal(res)) {
                    goto degrade;
                }
                /* reset res to 0 */
                res = 0;
            }
            ngx_queue_t *q = ngx_udpv2_active_writable_queue(ngx_xudp_get_tx());
            if (q != NULL) {
                ngx_post_event(wev, q);
                return res;
            }
        }
        /* degrade to system */
degrade:
        ngx_xudp_disable_tx(qc->connection);
        if (wev->posted) {
            ngx_delete_posted_event(wev);
        }
    }
#endif
#endif

    // 初始化消息头数组
    for(i = 0 ; i < vlen; i++){
        msg[i].msg_hdr.msg_iov = (struct iovec *) msg_iov + i;
        msg[i].msg_hdr.msg_iovlen = 1;
    }

    // 使用 sendmmsg 函数发送数据
    res = sendmmsg(fd, msg, vlen, 0);

    // 处理发送阻塞的情况
    if (res < 0 && (errno == EAGAIN)) {
        return ngx_http_xquic_on_write_block(qc, wev);
    } else if (res < 0) {
        // 处理其他发送错误情况
        ngx_log_error(NGX_LOG_WARN, ngx_cycle->log, 0,
            "|xquic|ngx_xquic_server_send_mmsg err|total_len=%z now=%i|dcid=%s|send_len=%z|errno=%s|",
            vlen, ngx_xquic_get_time(), xqc_dcid_str(&qc->dcid), res, strerror(errno));
        return XQC_SOCKET_ERROR;
    }

    ngx_log_error(NGX_LOG_DEBUG, ngx_cycle->log, 0,
            "|xquic|ngx_xquic_server_send_mmsg success|total_len=%z now=%i|dcid=%s|send_len=%z|",
            vlen, ngx_xquic_get_time(), xqc_dcid_str(&qc->dcid), res);


    return res;
}
#endif


static ngx_inline ssize_t
ngx_http_xquic_on_write_block(ngx_http_xquic_connection_t *qc, ngx_event_t *wev)
{
    // 获取 core 模块的配置信息
    ngx_http_core_loc_conf_t    *clcf;

    clcf    = ngx_http_get_module_loc_conf(qc->http_connection->conf_ctx,
                                        ngx_http_core_module);

    // 将写事件设置为未准备状态
    wev->ready = 0;

    // 处理写事件，设置写事件的 lowat
    if (ngx_handle_write_event(wev, clcf->send_lowat) != NGX_OK) {
        ngx_log_error(NGX_LOG_WARN, ngx_cycle->log, 0,
            "|xquic|ngx_handle_write_event err|");
        return XQC_SOCKET_ERROR;
    }
    // 返回再次尝试的标志
    return XQC_SOCKET_EAGAIN;
}
