/**
 * 
 */

#include <CUnit/CUnit.h>
#include "xquic/xquic.h"
#include "src/transport/xqc_conn.h"
#include "src/transport/xqc_client.h"
#include "xquic/xquic_typedef.h"
#include "src/common/xqc_str.h"
#include "src/congestion_control/xqc_new_reno.h"
#include "xqc_common_test.h"
#include "src/transport/xqc_engine.h"
#include "src/transport/xqc_switch.h"
#include "src/transport/xqc_ip_CCA_info.h"

void xqc_test_conn_CCA_hash_create()
{
  xqc_engine_t *engine = test_create_engine();
  CU_ASSERT(engine != NULL);
  CU_ASSERT(engine->container != NULL);
  xqc_engine_destroy(engine);
}

void xqc_test_mphash_create()
{
  xqc_engine_t *engine = test_create_engine();
  CU_ASSERT(engine != NULL);
  CU_ASSERT(engine->container != NULL);
  
  xqc_conn_settings_t conn_settings;
  memset(&conn_settings, 0, sizeof(xqc_conn_settings_t));
  conn_settings.proto_version = XQC_VERSION_V1;
  
  xqc_conn_ssl_config_t conn_ssl_config;
  memset(&conn_ssl_config, 0, sizeof(conn_ssl_config));
  xqc_connection_t *conn = xqc_client_connect(engine, &conn_settings, NULL, 0, "", 0, &conn_ssl_config,
                                       "transport", NULL, 0, NULL);
  CU_ASSERT(conn->mp_map != NULL)
  xqc_engine_destroy(engine);
}

