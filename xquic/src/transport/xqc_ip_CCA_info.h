// added by jndu for CCA switching
#ifndef _xqc_ip_H_INCLUDED
#define _xqc_ip_H_INCLUDED

#include <net/if.h>
#include <sys/socket.h>
#include <xquic/xquic_typedef.h>
#include <xquic/xquic.h>
#include "src/common/xqc_str_hash.h"
#include "src/transport/xqc_engine.h"
#include "src/transport/xqc_conn.h"
#include "src/transport/xqc_send_ctl.h"
#include <string.h>
#include <stdlib.h>

/**
 * @brief pass mp_hash to conn, find by ip+port.
*/
xqc_ip_CCA_info_t **
xqc_ip_get_mp_map (xqc_engine_t *engine, const struct sockaddr *sa_peer, socklen_t peer_addrlen);

/**
 * @brief get CCA_infos in send_ctl, find by path_id.
 * @return NULL: find nothing
*/
xqc_ip_CCA_info_t *
xqc_ip_get_CCA_info (xqc_send_ctl_t *send_ctl);



#endif