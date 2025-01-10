// added by jndu
#include <net/if.h>
#include <sys/socket.h>
#include "src/transport/xqc_ip_CCA_info.h"
#include "src/transport/xqc_engine.h"
#include "src/transport/xqc_utils.h"
#include "src/transport/xqc_switch.h"
#include "src/common/xqc_log.h"
#include "src/common/xqc_str_hash.h"
#include "src/common/xqc_malloc.h"

xqc_ip_CCA_info_t **
xqc_ip_get_mp_map (xqc_engine_t *engine, const struct sockaddr *sa_peer, socklen_t peer_addrlen)
{
  xqc_ip_CCA_info_t **mp_map = NULL;
  xqc_CCA_info_container_t *container = engine->container;
  if (engine->eng_callback.get_mp_cb) {
    /* find by ip+port in conn_CCA_hash */
    mp_map = engine->eng_callback.get_mp_cb(container, sa_peer, peer_addrlen);
  } else {
    /* CCA infos only in stream level */
    mp_map = malloc(sizeof(xqc_ip_CCA_info_t *) * XQC_MAX_PATHS_COUNT);
    memset(mp_map, 0, sizeof(xqc_ip_CCA_info_t *) * XQC_MAX_PATHS_COUNT);
  }
  return mp_map;
}

xqc_ip_CCA_info_t *
xqc_ip_get_CCA_info(xqc_send_ctl_t *send_ctl)
{
  xqc_connection_t *conn = send_ctl->ctl_conn;
  xqc_engine_t *engine = conn->engine;
  xqc_ip_CCA_info_t *CCA_infos = NULL;
  int index = -1;

  /* get path index from mp_index (default 0) */
  index = send_ctl->mp_index;
  
  if (index <= -1 || index > XQC_MAX_PATHS_COUNT)
  {
    return NULL;
  }
  /* return CCA_info find by CCA_id */
  if (conn->mp_map == NULL) {
    xqc_log(conn->log, XQC_LOG_ERROR, "mp_map is NULL");
    return NULL;
  }
  CCA_infos = conn->mp_map[index];
  
  /**
   * if cant find CCA_infos, insert a new one
   */
  if (CCA_infos == NULL)
  {
    CCA_infos = xqc_malloc(sizeof(xqc_ip_CCA_info_t) * XQC_CCA_NUM);
    xqc_memset(CCA_infos, 0, sizeof(xqc_ip_CCA_info_t) * XQC_CCA_NUM);
    conn->mp_map[index] = CCA_infos;
  }
  return CCA_infos;
}
