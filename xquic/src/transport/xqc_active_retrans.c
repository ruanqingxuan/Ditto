// added by qnwang for AR
#define _USE_MATH_DEFINES
#include <math.h>
#include <complex.h>
#include "src/transport/xqc_active_retrans.h"
#include "src/transport/xqc_packet.h"
#include "src/common/xqc_extra_log.h"
#include "src/transport/xqc_conn.h"
#include "src/common/xqc_log.h"
#include "src/common/xqc_log_event_callback.h"

#define XQC_DEFAULT_MAX_UDP_PAYLOAD_SIZE 65527

// 在conn初始化的时候调用 conn
xqc_active_retrans_t *xqc_active_retrans_init() // xqc_log_t *log, xqc_extra_log_t *AR_extra_log
{
    xqc_active_retrans_t *active = (xqc_active_retrans_t *)xqc_malloc(sizeof(xqc_active_retrans_t));
    active->success_probability = 0.999;
    active->active_num = 1;
    active->flag = XQC_FALSE;
    active->avg_lossrate = 0;
    active->loss_rate = 0;
    active->loss_weight = 0.5;
    active->threshold = 0.9;
    return active;
}

// 在更新lossrate的时候调用 extra_sample
void xqc_update_avg_lossrate(xqc_connection_t *conn, float lossrate)
{
    conn->active_retrans->loss_rate = lossrate;
    float weight = conn->active_retrans->loss_weight;
    float avg_lossrate = conn->active_retrans->avg_lossrate;
    conn->active_retrans->avg_lossrate = weight * lossrate + (1 - weight) * avg_lossrate;

    xqc_update_active_num(conn);
    xqc_extra_log(conn->log, conn->AR_extra_log, "[update lossrate:%.5f] [avg_lossrate:%.5f]",
                  conn->active_retrans->loss_rate, conn->active_retrans->avg_lossrate);
    xqc_log(conn->log, XQC_LOG_INFO, "|Active Retransmission|update avg_lossrate %.5f|\n",
            conn->active_retrans->avg_lossrate);
}

// 在更新lastest_rtt的时候调用 send_ctl
int xqc_update_success_probability(xqc_connection_t *conn, xqc_usec_t rtt)
{
    // calculate t/RTT
    float proportion = (float)(conn->conn_settings.expected_time * 1000) / rtt;
    // 3x^3-8x^2+6x
    // float extremum = (272 + 20 * sqrt(10)) / 243;
    // 向下取整
    int result = (int)floor(proportion);
    // 用户有要求
    if (conn->conn_settings.expected_time > 0)
    {
        conn->active_retrans->flag = XQC_TRUE;
        // success=0.999
        if (result == 0)
        {
            xqc_update_active_num(conn);
        }
        // update success
        if (result > 0)
        {
            conn->active_retrans->success_probability = 1 - pow(conn->active_retrans->avg_lossrate, result);
            xqc_update_active_num(conn);
        }
    }
    xqc_extra_log(conn->log, conn->AR_extra_log, "[expected_time:%ui] [rtt:%ui] [result:%ui]", conn->conn_settings.expected_time, rtt / 1000, result);
    xqc_log(conn->log, XQC_LOG_INFO, "|Active Retransmission|update success flag:%ui|\n",
            conn->active_retrans->flag);
    return XQC_OK;
}

float xqc_cardano_solution(xqc_connection_t *conn, int a, int b, int c, float d)
{
    // 将三次方程标准化：x^3 + px + q = 0
    float p = (float)(3 * a * c - b * b) / (3 * a * a);
    float q = (float)(2 * b * b * b - 9 * a * b * c + 27 * a * a * d) / (27 * a * a * a);
    float delta = (q * q / 4) + (p * p * p / 27);
    xqc_extra_log(conn->log, conn->AR_extra_log, "[cardano a:%d] [b:%d] [c:%d] [d:%.5f] [delta:%.5f]", a, b, c, d, delta);

    if (delta > 0)
    {
        // 只有一个实根
        float u = cbrt(-q / 2 + sqrt(delta));
        float v = cbrt(-q / 2 - sqrt(delta));
        float x1 = u + v - b / (3 * a);
        xqc_extra_log(conn->log, conn->AR_extra_log, "[cardano success x1:%.5f]", x1);
        return x1;
        // printf("唯一实根: x = %lf\n", x1);
    }
    else if (delta == 0)
    {
        // 三次方程有三个实根，其中至少两个相等
        float x1 = 3 * q / p - b / (3 * a);
        float x2 = -3 * q / (2 * p) - b / (3 * a);
        if (x1 < 1 && x1 > 0)
        {
            if (x2 < 1 && x2 > 0)
            {
                if (x1 >= x2)
                {
                    return x1;
                }
                else
                {
                    return x2;
                }
            }
            else
            {
                return x1;
            }
        }
        else if (x2 < 1 && x2 > 0)
        {
            return x2;
        }
        else
        {
            xqc_extra_log(conn->log, conn->AR_extra_log, "[cardano success error2]");
            return 0.99;
        }
        // printf("实根: x1 = %lf, x2 = %lf\n", x1, x2);
        xqc_extra_log(conn->log, conn->AR_extra_log, "[cardano success x1:%.5f] [x2:%.5f]", x1, x2);
    }
    else
    {
        // 有三个不相等的实根
        float r = sqrt(-p * p * p / 27);
        float theta = acos(-q / (2 * r));
        float x1 = 2 * cbrt(r) * cos(theta / 3) - b / (3 * a);
        float x2 = 2 * cbrt(r) * cos((theta + 2 * M_PI) / 3) - b / (3 * a);
        float x3 = 2 * cbrt(r) * cos((theta + 4 * M_PI) / 3) - b / (3 * a);
        if (x1 < 1 && x1 > 0)
        {
            if (x2 < 1 && x2 > 0)
            {
                if (x1 >= x2)
                {
                    if (x3 < 1 && x3 > 0)
                    {
                        if (x1 >= x3)
                        {
                            return x1;
                        }
                        else
                        {
                            return x3;
                        }
                    }
                    else
                    {
                        return x1;
                    }
                }
                else
                {
                    if (x3 < 1 && x3 > 0)
                    {
                        if (x2 >= x3)
                        {
                            return x2;
                        }
                        else
                        {
                            return x3;
                        }
                    }
                    else
                    {
                        return x2;
                    }
                }
            }
            else
            {
                if (x3 < 1 && x3 > 0)
                {
                    if (x1 >= x3)
                    {
                        return x1;
                    }
                    else
                    {
                        return x3;
                    }
                }
                else
                {
                    return x1;
                }
            }
        }
        else if (x2 < 1 && x2 > 0)
        {
            if (x3 < 1 && x3 > 0)
            {
                if (x2 >= x3)
                {
                    return x2;
                }
                else
                {
                    return x3;
                }
            }
            else
            {
                return x2;
            }
        }
        else if (x3 < 1 && x3 > 0)
        {
            return x3;
        }
        else
        {
            xqc_extra_log(conn->log, conn->AR_extra_log, "[cardano success error3]");
            return 0.99;
        }
        xqc_extra_log(conn->log, conn->AR_extra_log, "[cardano success x1:%.5f] [x2:%.5f] [x3:%.5f]", x1, x2, x3);
        // printf("三个不相等的实根: x1 = %lf, x2 = %lf, x3 = %lf\n", x1, x2, x3);
    }
}

void xqc_update_active_num(xqc_connection_t *conn)
{
    float ar_num = log((1 - conn->active_retrans->threshold)) / log((1 - conn->active_retrans->success_probability));
    conn->active_retrans->active_num = xqc_max((int)ceil(ar_num), 1);
    if (conn->active_retrans->active_num > 8)
    {
        conn->active_retrans->active_num = 1;
    }
    xqc_extra_log(conn->log, conn->AR_extra_log, "[update num:%ui]", conn->active_retrans->active_num);
    xqc_log(conn->log, XQC_LOG_INFO, "|Active Retransmission|update active_num:%ui|\n", conn->active_retrans->active_num);
}
// 在conn初始化的时候调用 conn
xqc_redundant_pkt_t *xqc_redundant_pkt_init() // xqc_log_t *log, xqc_extra_log_t *AR_extra_log
{
    xqc_redundant_pkt_t *re_pkt = (xqc_redundant_pkt_t *)xqc_malloc(sizeof(xqc_redundant_pkt_t));
    size_t size = 65527;
    re_pkt->pkt_num = 0;
    re_pkt->pkt_pns = 0;
    re_pkt->pkt_sent_time = 0;
    re_pkt->pkt_payload = (unsigned char *)malloc(size);
    re_pkt->next = NULL;
    return re_pkt;
}

void xqc_get_packet_out(xqc_connection_t *conn, xqc_packet_out_t *packet_out)
{
    xqc_redundant_pkt_t *p;
    xqc_packet_out_t *re_p;
    p = conn->redundant_pkt;
    re_p = packet_out;
    // conn-type
    const char *conn_type[] = {"XQC_CONN_TYPE_CLIENT", "XQC_CONN_TYPE_SERVER"};
    // pns-type
    const char *pns[] = {"XQC_PNS_INIT", "XQC_PNS_HSK", "XQC_PNS_APP_DATA", "XQC_PNS_N"};
    while (p != NULL)
    {
        if (conn->conn_settings.expected_time / 100 >= 1 && conn->conn_settings.expected_time / 100 < 2)
        {
            if (p->pkt_payload == re_p->po_payload && (re_p->po_flag & XQC_POF_RETRANSED || re_p->po_flag & XQC_POF_RESEND || re_p->po_flag & XQC_POF_LOST)) //
            {
                // xqc_extra_log(conn->log, conn->AR_extra_log, "[pkt_num:%ui] [re_num:%ui] [conn_type:%s] [pns:%s] [send_time:%ui] [re_time:%ui]",
                //               p->pkt_num, re_p->po_pkt.pkt_num, conn_type[conn->conn_type], pns[p->pkt_pns], p->pkt_sent_time, re_p->po_sent_time);
                re_p->po_sent_time = p->pkt_sent_time;
            }
        }
        else
        {
            if (p->pkt_payload == re_p->po_payload)
            {
                re_p->po_sent_time = p->pkt_sent_time;
            }
        }

        // 假设保存第一个包号
        if (p->pkt_pns == XQC_PNS_INIT)
        {
            p->pkt_num = re_p->po_pkt.pkt_num;
            p->pkt_pns = re_p->po_pkt.pkt_pns;
            p->pkt_sent_time = re_p->po_sent_time;
            p->pkt_payload = re_p->po_payload;
            p->next = xqc_redundant_pkt_init();
            xqc_extra_log(conn->log, conn->AR_extra_log, "[pkt_num:%ui] [active_num:%ui] [conn_type:%s] [pns:%s] [send_time:%ui]",
                          p->pkt_num, conn->active_retrans->active_num, conn_type[conn->conn_type], pns[p->pkt_pns], p->pkt_sent_time);
            xqc_log(conn->log, XQC_LOG_INFO, "|Redundant Packet_send|pkt_num:%ui|\n", p->pkt_num);
            break;
        }
        else
        {
            p = p->next;
        }
    }
}

// 放在conn destroy里
void xqc_destroy_reduntant_pkt(xqc_redundant_pkt_t *re_pkt)
{
    xqc_redundant_pkt_t *p = re_pkt;
    while (p != NULL)
    {
        re_pkt = re_pkt->next;
        xqc_free(p);
        p = re_pkt;
    }
};