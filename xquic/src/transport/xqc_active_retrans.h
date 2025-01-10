// add for AR qnwang

#include "src/transport/xqc_conn.h"
#include "src/transport/xqc_multipath.h"
#include <xquic/xquic_typedef.h>

typedef struct xqc_active_retrans_s
{
    float loss_rate;
    float avg_lossrate;
    int active_num;
    float loss_weight;
    float success_probability;
    xqc_bool_t flag;
    float threshold;
} xqc_active_retrans_t;

typedef struct xqc_redundant_pkt_s
{
    xqc_packet_number_t pkt_num;
    xqc_pkt_num_space_t pkt_pns;
    xqc_usec_t pkt_sent_time; /* microsecond */
    unsigned char *pkt_payload; 
    xqc_redundant_pkt_t *next;
} xqc_redundant_pkt_t;

/**
 * @brief initialize one active_retrans struct.
 */
xqc_active_retrans_t *xqc_active_retrans_init();

/**
 * @brief initialize one redundant_pkt struct.
 */
xqc_redundant_pkt_t *xqc_redundant_pkt_init();

/**
 * @brief get command about expected_time and weight.
 */
void xqc_get_expected_time(xqc_connection_t *conn, xqc_msec_t time);

/**
 * @brief read lossrate and update avg_lossrate.
 */
void xqc_update_avg_lossrate(xqc_connection_t *conn, float lossrate);

/**
 * @brief read RTT and update success_probability.
 * @return 0 for error, 1 for success
 */
int xqc_update_success_probability(xqc_connection_t *conn, xqc_usec_t rtt);

/**
 * @brief caclulate success_probability.
 * @return success_probability
 */
float xqc_cardano_solution(xqc_connection_t *conn, int a, int b, int c, float d);

/**
 * @brief update active_num.
 */
void xqc_update_active_num(xqc_connection_t *conn);

/**
 * @brief update re_pkt.pkt_num+send_time.
 */
void xqc_get_packet_out(xqc_connection_t *conn, xqc_packet_out_t *packet_out);

/**
 * @brief update lost packet
 */
void xqc_get_packet_lost(xqc_connection_t *conn, xqc_packet_out_t *packet_out);

/**
 * @brief destroy reduntant_pkt.
 */
void xqc_destroy_reduntant_pkt(xqc_redundant_pkt_t *p);
