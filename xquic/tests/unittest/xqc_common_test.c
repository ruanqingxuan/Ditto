/**
 * @copyright Copyright (c) 2022, Alibaba Group Holding Limited
 */

#include <CUnit/CUnit.h>

#include <xquic/xquic.h>
#include <xquic/xqc_http3.h>
#include <net/if.h>
#include <sys/socket.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include "xqc_common_test.h"
#include "src/common/xqc_object_manager.h"
#include "src/common/xqc_rbtree.h"
#include "src/common/xqc_fifo.h"


typedef struct person_s {
    int age;
    char name[20];
    xqc_queue_t queue;
} person_t;

typedef struct xqc_item_s {
    xqc_object_id_t object_id;
    xqc_list_head_t list;
    int data;
} xqc_item_t;

static inline void
test_object_manager_cb(xqc_object_t *o)
{
    /*xqc_item_t* item = (xqc_item_t*)o;
    printf("id:%u, data:%d\n", item->object_id, item->data);*/
}

int
test_object_manager()
{
    xqc_object_manager_t *manager = xqc_object_manager_create(sizeof(xqc_item_t), 4, xqc_default_allocator);
    if (manager == NULL) {
        return 1;
    }

    xqc_item_t *item1 = (xqc_item_t*)xqc_object_manager_alloc(manager);
    if (item1) {
        item1->data = 1;
    }

    xqc_item_t *item2 = (xqc_item_t*)xqc_object_manager_alloc(manager);
    if (item2) {
        item2->data = 2;
        xqc_object_manager_free(manager, item2->object_id);
    }

    xqc_item_t *item3 = (xqc_item_t*)xqc_object_manager_alloc(manager);
    if (item3) {
        item3->data = 3;
    }

    xqc_object_manager_foreach(manager, test_object_manager_cb);

    CU_ASSERT(xqc_object_manager_used_count(manager) == 2);
    CU_ASSERT(xqc_object_manager_free_count(manager) == 2);

    xqc_object_manager_destroy(manager);

    return 0;
}

static inline void
rbtree_cb(xqc_rbtree_node_t* node)
{
    //printf("key=%lu\n", (unsigned long)node->key);
}

int
test_rbtree()
{
    xqc_rbtree_t rbtree;
    xqc_rbtree_init(&rbtree);

    xqc_rbtree_node_t list[] = {
        { 0, 0, 0, 5, xqc_rbtree_black },
        { 0, 0, 0, 1, xqc_rbtree_black },
        { 0, 0, 0, 4, xqc_rbtree_black },
        { 0, 0, 0, 7, xqc_rbtree_black },
        { 0, 0, 0, 8, xqc_rbtree_black },
        { 0, 0, 0, 9, xqc_rbtree_black },
        { 0, 0, 0, 2, xqc_rbtree_black },
        { 0, 0, 0, 0, xqc_rbtree_black },
        { 0, 0, 0, 3, xqc_rbtree_black },
        { 0, 0, 0, 6, xqc_rbtree_black },
    };

    for (size_t i = 0; i < sizeof(list)/sizeof(*list); ++i) {
        xqc_rbtree_insert(&rbtree, &list[i]);
    }

    xqc_rbtree_insert(&rbtree, &list[1]);

    xqc_rbtree_node_t* p = xqc_rbtree_find(&rbtree, 6);
    if (p) {
        //printf("found 6\n");
    }

    p = xqc_rbtree_find(&rbtree, 16);
    if (!p) {
        //printf("not found 16\n");
    }

    xqc_rbtree_delete(&rbtree, 7);
    xqc_rbtree_delete(&rbtree, 1);
    xqc_rbtree_delete(&rbtree, 9);

    CU_ASSERT(xqc_rbtree_count(&rbtree) == 7);

    xqc_rbtree_foreach(&rbtree, rbtree_cb);
    return 0;
}

void
xqc_test_common()
{
    /* test queue */
    xqc_queue_t q;
    xqc_queue_init(&q);
    person_t p1 = { 1, "a1", xqc_queue_initialize(p1.queue) };
    person_t p2 = { 2, "z2", xqc_queue_initialize(p2.queue) };
    person_t p3 = { 3, "s3", xqc_queue_initialize(p3.queue) };
    person_t p4 = { 4, "f4", xqc_queue_initialize(p4.queue) };

    xqc_queue_insert_head(&q, &p1.queue);
    xqc_queue_insert_tail(&q, &p2.queue);
    xqc_queue_insert_head(&q, &p3.queue);
    xqc_queue_insert_tail(&q, &p4.queue);
    xqc_queue_remove(&p3.queue);

    int a[4] = {1,2,4};
    int i = 0;

    xqc_queue_t *pos;
    xqc_queue_foreach(pos, &q) {
        person_t* p = xqc_queue_data(pos, person_t, queue);
        CU_ASSERT(p->age == a[i]);
        ++i;
    }

    /* test hash functions */
    xqc_md5_t ctx;
    xqc_md5_init(&ctx);
    unsigned char buf[] = "hello,world";
    xqc_md5_update(&ctx, buf, 11);

    unsigned char final[16] = {};
    xqc_md5_final(final, &ctx);

    uint32_t hash_value = xqc_murmur_hash2(buf, 11);

    test_object_manager();

    test_rbtree();

    /* test fifo */
    xqc_fifo_t fifo;
    memset(&fifo, 0, sizeof(fifo));
    xqc_fifo_init(&fifo, xqc_default_allocator, sizeof(int), 4);

    xqc_fifo_push_int(&fifo, 3);
    xqc_fifo_push_int(&fifo, 1);
    xqc_fifo_push_int(&fifo, 2);
    xqc_fifo_push_int(&fifo, 4);

    xqc_fifo_pop(&fifo);
    xqc_fifo_pop(&fifo);
    xqc_fifo_push_int(&fifo, 9);
    xqc_fifo_push_int(&fifo, 7);
    xqc_fifo_pop(&fifo);
    xqc_fifo_push_int(&fifo, 5);

    CU_ASSERT(xqc_fifo_length(&fifo) == 4);

    while (xqc_fifo_empty(&fifo) != XQC_TRUE) {
        int i = xqc_fifo_top_int(&fifo);
        //printf("%d, length:%d\n", i, xqc_fifo_length(&fifo));
        xqc_fifo_pop(&fifo);
    }

    xqc_fifo_release(&fifo);
}


#define def_engine_ssl_config   \
xqc_engine_ssl_config_t  engine_ssl_config;             \
engine_ssl_config.private_key_file = "./server.key";    \
engine_ssl_config.cert_file = "./server.crt";           \
engine_ssl_config.ciphers = XQC_TLS_CIPHERS;            \
engine_ssl_config.groups = XQC_TLS_GROUPS;              \
engine_ssl_config.session_ticket_key_len = 0;           \
engine_ssl_config.session_ticket_key_data = NULL;

static ssize_t
null_socket_write(const unsigned char *buf, size_t size,
    const struct sockaddr *peer_addr,
    socklen_t peer_addrlen, void *conn_user_data)
{
    return size;
}

static ssize_t
null_socket_write_ex(uint64_t path_id,
    const unsigned char *buf, size_t size,
    const struct sockaddr *peer_addr,
    socklen_t peer_addrlen, void *conn_user_data)
{
    return size;
}

static void
null_set_event_timer(xqc_msec_t wake_after, void *engine_user_data)
{
    return;
}

void
xqc_write_log_default(xqc_log_level_t lvl, const void *buf, size_t count, void *user_data)
{
    return;
}

const xqc_log_callbacks_t xqc_null_log_cb = {
    .xqc_log_write_err = xqc_write_log_default,
};

/**
 * added by jndu
 */
#define XQC_DEMO_MAX_PATH_COUNT    8
char g_multi_interface[XQC_DEMO_MAX_PATH_COUNT][64];
int g_enable_multipath = 0;
int g_multi_interface_cnt = 8;

char g_local_addr_str[INET6_ADDRSTRLEN];
char g_peer_addr_str[INET6_ADDRSTRLEN];

char *
xqc_local_addr2str(const struct sockaddr *local_addr, socklen_t local_addrlen)
{
    if (local_addrlen == 0 || local_addr == NULL) {
        g_local_addr_str[0] = '\0';
        return g_local_addr_str;
    }

    struct sockaddr_in *sa_local = (struct sockaddr_in *)local_addr;
    if (sa_local->sin_family == AF_INET) {
        if (inet_ntop(sa_local->sin_family, &sa_local->sin_addr, g_local_addr_str, local_addrlen) == NULL) {
            g_local_addr_str[0] = '\0';
        }

    } else {
        if (inet_ntop(sa_local->sin_family, &((struct sockaddr_in6*)sa_local)->sin6_addr,
                      g_local_addr_str, local_addrlen) == NULL)
        {
            g_local_addr_str[0] = '\0';
        }
    }

    return g_local_addr_str;
}


char *
xqc_peer_addr2str(const struct sockaddr *peer_addr, socklen_t peer_addrlen)
{
    if (peer_addrlen == 0 || peer_addr == NULL) {
        g_peer_addr_str[0] = '\0';
        return g_peer_addr_str;
    }

    struct sockaddr_in *sa_peer = (struct sockaddr_in *)peer_addr;
    if (sa_peer->sin_family == AF_INET) {
        if (inet_ntop(sa_peer->sin_family, &sa_peer->sin_addr, g_peer_addr_str, peer_addrlen) == NULL) {
            g_peer_addr_str[0] = '\0';
        }

    } else {
        if (inet_ntop(sa_peer->sin_family, &((struct sockaddr_in6*)sa_peer)->sin6_addr,
                      g_peer_addr_str, peer_addrlen) == NULL)
        {
            g_peer_addr_str[0] = '\0';
        }
    }

    return g_peer_addr_str;
}

void *xqc_client_get_mp_by_addr(xqc_CCA_info_container_t *container, const struct sockaddr *sa_peer, socklen_t peer_addrlen) 
{
  char addr_str[2 * (INET6_ADDRSTRLEN) + 10];
  memset(addr_str, 0, sizeof(addr_str));
  size_t addr_len = snprintf(addr_str, sizeof(addr_str), "l-%s-%d p-%s-%d",
                                      "127.0.0.1", 10,
                                      xqc_peer_addr2str((struct sockaddr*)sa_peer, peer_addrlen),
                                      10);

  xqc_ip_CCA_info_t **mp_map = xqc_CCA_info_container_find(container, addr_str, addr_len);
  if (!mp_map) {
    mp_map = malloc(sizeof(xqc_ip_CCA_info_t *) * XQC_MAX_PATHS_COUNT);
    memset(mp_map, 0, sizeof(xqc_ip_CCA_info_t *) * XQC_MAX_PATHS_COUNT);
    if (xqc_CCA_info_container_add(container,
                                   addr_str, addr_len, mp_map))
    {
      xqc_free(mp_map);
      mp_map = NULL;
    }
  }
  return mp_map;
}

int xqc_client_get_path_index_by_if(unsigned char* name) 
{
    if (!g_enable_multipath) {
        return 0;
    }

    for (int i = 0; i < g_multi_interface_cnt; i++) {
        if (!memcmp(g_multi_interface[i], name, strlen(name))) {
            return i;
        }
    }

    return -1;
}

int xqc_client_get_path_idx_by_id(uint64_t path_id)
{

    if (!g_enable_multipath) {
        return 0;
    }

    for (int i = 0; i < g_multi_interface_cnt; i++) {
        return i;
    }

    return -1;
}

/* literal */
const xmlChar *IPCONF_ = "ip";
const xmlChar *CONF_ = "conf";
const xmlChar *ADDR = "addr";
const xmlChar *INTERFACE = "interface";
const xmlChar *IF = "if";
const xmlChar *CCA = "CCA";
const xmlChar *ID = "id";
const xmlChar *CNT = "cnt";
const xmlChar *POWER1 = "power1";
const xmlChar *POWER2 = "power2";
const xmlChar *POWER3 = "power3";

xqc_int_t
xqc_load_CCA_info (xmlDocPtr doc, xmlNodePtr cur, xqc_ip_CCA_info_t *CCA_info) {

  memset(CCA_info, 0, sizeof(xqc_ip_CCA_info_t) * XQC_CCA_NUM);
  xmlNodePtr cur_child = cur->children;
  xmlChar *id;
  if (cur == NULL) {
    return -1;
  }

  /* traverse CCA info */
  while (cur_child != NULL) {
    if (xmlStrcmp(cur_child->name, CCA) == 0) {
        id = xmlGetProp(cur_child, "id");
        int id_i = atoi(id);
        xqc_ip_CCA_info_t *info = CCA_info + id_i;

        /* put nums from xml file to CCA_info */
        xmlNodePtr child_child = cur_child->children;
        while (child_child != NULL) {
            char* endptr;
            if (!xmlStrcmp(child_child->name, CNT)) {
                endptr = xmlNodeListGetString(doc, child_child->children, 0);
                info->CCA_cnt = atoi((const char*)xmlNodeListGetString(doc, child_child->children, 0));
            }
            if (!xmlStrcmp(child_child->name, POWER1)) {
                info->CCA_power1 = strtod((const char*)xmlNodeListGetString(doc, child_child->children, 0), &endptr);
            }
            if (!xmlStrcmp(child_child->name, POWER2)) {
                info->CCA_power2 = strtod((const char*)xmlNodeListGetString(doc, child_child->children, 0), &endptr);
            }
            if (!xmlStrcmp(child_child->name, POWER3)) {
                info->CCA_power3 = strtod((const char*)xmlNodeListGetString(doc, child_child->children, 0), &endptr);
            }
            child_child = child_child->next;
        }
    }
    cur_child = cur_child->next;
  }

  return 0;
}

/**
 * @brief read ip XML file and create hash table in current engine.
 * @return 0 for can read config file and successfully initialized
 * @return 1 for cant read config file and return a blank hash
 * @return -1 for memory fatal error
 * @bug no
*/
xqc_int_t
xqc_load_CCA_param (xqc_CCA_info_container_t *container, const char* file) {
  xmlDocPtr doc;
  xmlNodePtr cur;
  /* for 1-level node, parsing the ip, port and interface. */
  xmlChar *addr, *if_;
  /* for 2-level node, parsing the different CCA. */
  xmlChar *id;

  /* get the tree. */
  doc = xmlReadFile(file, NULL, 0);
  if (doc == NULL) {
    goto FAILED;
  }

  /* get the root. */
  cur = xmlDocGetRootElement(doc);
  if (cur == NULL) {
    goto FAILED;
  }

  if (xmlStrcmp(cur->name, IPCONF_) != 0) {
    goto FAILED;
  }

  /* traverse all the node. */
  cur = cur->children;
  /* traverse by ip+port */
  while (cur != NULL) {
    if (xmlStrcmp(cur->name, CONF_) == 0) {
      addr = xmlGetProp(cur, ADDR);

      xmlNodePtr cur_child = cur->children;
      /* traverse by interface and add mp_map to conn_CCA_hash */
      xqc_ip_CCA_info_t **mp_map = malloc(sizeof(xqc_ip_CCA_info_t*) * XQC_MAX_PATHS_COUNT);
      memset(mp_map, 0, sizeof(xqc_ip_CCA_info_t*) * XQC_MAX_PATHS_COUNT);
      while (cur_child != NULL) {
        if (xmlStrcmp(cur_child->name, INTERFACE) == 0) {
          if_ = xmlGetProp(cur_child, IF);
          int index = xqc_client_get_path_index_by_if(if_);
          if (index >= 0) {
            xqc_ip_CCA_info_t *CCA_info = malloc(sizeof(xqc_ip_CCA_info_t) * XQC_CCA_NUM);
            xqc_load_CCA_info(doc, cur_child, CCA_info);
            mp_map[index] = CCA_info;
          }
        }
        cur_child = cur_child->next;
      }

      /* add mp_hash to conn_CCA_hash */
      if (xqc_CCA_info_container_add(container, addr, strlen(addr), mp_map)) {
        free(mp_map);
        return -1;
      }

    }
    cur = cur->next;
  }

  return 0;
FAILED:
  return 1;
}

xqc_int_t
xqc_save_CCA_info (xmlNodePtr ip_node, xqc_ip_CCA_info_t **mp_map) {

  for (size_t i = 0; i < g_multi_interface_cnt; i++)
  {
    xqc_ip_CCA_info_t *CCA_info = mp_map[i];

    if (CCA_info) {
      /* add interface node */
      xmlNodePtr if_node = xmlNewNode(NULL, INTERFACE);
      if (if_node == NULL) {
        return -1;
      }

      
      xmlNewProp(if_node, IF, g_multi_interface[i]);

      /* add CCA_info to interface node */
      for (int j = 0; j < XQC_CCA_NUM; j++)
      {
        if (CCA_info[j].CCA_cnt == 0) {
          continue;
        }
        unsigned char content[10];
        memset(content, 0, 10);

        xmlNodePtr CCA_node = xmlNewNode(NULL, CCA);
        sprintf(content, "%d", j);
        xmlNewProp(CCA_node, ID, content);
        if (CCA_node == NULL) {
          goto FAILED;
        }

        memset(content, 0, 10);
        sprintf(content, "%d", CCA_info[j].CCA_cnt);
        if (xmlNewChild(CCA_node, NULL, CNT, content) == NULL ) {
          goto FAILED;
        }

        memset(content, 0, 10);
        sprintf(content, "%f", CCA_info[j].CCA_power1);
        if (xmlNewChild(CCA_node, NULL, POWER1, content) == NULL ) {
          goto FAILED;
        }

        memset(content, 0, 10);
        sprintf(content, "%f", CCA_info[j].CCA_power2);
        if (xmlNewChild(CCA_node, NULL, POWER2, content) == NULL ) {
          goto FAILED;
        }

        memset(content, 0, 10);
        sprintf(content, "%f", CCA_info[j].CCA_power3);
        if (xmlNewChild(CCA_node, NULL, POWER3, content) == NULL ) {
          goto FAILED;
        }

        xmlAddChild(if_node, CCA_node);
      }

      xmlAddChild(ip_node, if_node);

    }
  }
  return 0;

FAILED:
  return -1;
}

xqc_int_t
xqc_save_CCA_param (xqc_CCA_info_container_t *container, const char* file) {
  xmlDocPtr doc = NULL;
  xmlNodePtr root = NULL;

  /* create a new xml file */
  doc = xmlNewDoc(BAD_CAST"1.0");
  if (doc == NULL) {
    goto FAILED;
  }

  /* create a root node */
  root = xmlNewNode(NULL, IPCONF_);
  if (root == NULL) {
    goto FAILED;
  }

  /* add root to doc */
  xmlDocSetRootElement(doc, root);

  /* traverse conn_CCA_hash */

  xqc_ip_CCA_info_t **mp_map;
  char addr[2 * (INET6_ADDRSTRLEN) + 10];
  size_t addr_len;
  while ((mp_map = xqc_CCA_info_container_get(container, addr, &addr_len))) {
    /* add ip+port node */
    xmlNodePtr ip_node = xmlNewNode(NULL, CONF_);
    if (ip_node == NULL) {
      goto FAILED;
    }
    xmlNewProp(ip_node, ADDR, addr);
    /* add interface node */
    if (xqc_save_CCA_info(ip_node, mp_map)) {
      goto FAILED;
    }
    xqc_CCA_info_container_free_value(mp_map);
    /* add ip+port node to root */
    xmlAddChild(root, ip_node);
    xqc_CCA_info_container_delete(container, addr, addr_len);
  }

  /* save doc to xml file */
  xmlSaveFormatFileEnc(file, doc, "UTF-8", 1);
  xmlFreeDoc(doc);

  return 0;
FAILED:
  if (doc) {
    	xmlFreeDoc(doc);
  }

  return -1;
}

xqc_engine_t *
test_create_engine()
{
    def_engine_ssl_config;
    xqc_engine_callback_t callback = {
        .log_callbacks = xqc_null_log_cb,
        .set_event_timer = null_set_event_timer,
        .load_CCA_cb = xqc_load_CCA_param,
        .save_CCA_cb = xqc_save_CCA_param,
        .get_mp_index_cb = xqc_client_get_path_idx_by_id,
        .get_mp_cb = xqc_client_get_mp_by_addr
    };
    xqc_transport_callbacks_t tcbs = {
        .write_socket = null_socket_write,
        .write_socket_ex = null_socket_write_ex,
    };

    xqc_conn_settings_t conn_settings;
    xqc_engine_t *engine = xqc_engine_create(XQC_ENGINE_CLIENT, NULL, &engine_ssl_config,
                                             &callback, &tcbs, NULL);

    xqc_h3_callbacks_t h3_cbs = {
        .h3c_cbs = {
            .h3_conn_create_notify = NULL,
            .h3_conn_close_notify = NULL,
            .h3_conn_handshake_finished = NULL,
        },
        .h3r_cbs = {
            .h3_request_create_notify = NULL,
            .h3_request_close_notify = NULL,
            .h3_request_read_notify = NULL,
            .h3_request_write_notify = NULL,
        }
    };

    /* init http3 context */
    int ret = xqc_h3_ctx_init(engine, &h3_cbs);
    if (ret != XQC_OK) {
        xqc_engine_destroy(engine);
        return NULL;
    }

    /* transport ALPN */
    xqc_app_proto_callbacks_t transport_cbs = {{NULL}, {NULL}};
    xqc_engine_register_alpn(engine, "transport", 9, &transport_cbs);

    return engine;
}

xqc_engine_t *
test_create_engine_server()
{
    def_engine_ssl_config;
    xqc_engine_callback_t callback = {
        .log_callbacks = xqc_null_log_cb,
        .set_event_timer = null_set_event_timer,
    };
    xqc_transport_callbacks_t tcbs = {
        .write_socket = null_socket_write,
    };

    xqc_conn_settings_t conn_settings;
    xqc_engine_t *engine = xqc_engine_create(XQC_ENGINE_SERVER, NULL, &engine_ssl_config,
                                             &callback, &tcbs, NULL);

    xqc_h3_callbacks_t h3_cbs = {
        .h3c_cbs = {
            .h3_conn_create_notify = NULL,
            .h3_conn_close_notify = NULL,
            .h3_conn_handshake_finished = NULL,
        },
        .h3r_cbs = {
            .h3_request_create_notify = NULL,
            .h3_request_close_notify = NULL,
            .h3_request_read_notify = NULL,
            .h3_request_write_notify = NULL,
        }
    };

    /* init http3 context */
    int ret = xqc_h3_ctx_init(engine, &h3_cbs);
    if (ret != XQC_OK) {
        xqc_engine_destroy(engine);
        return NULL;
    }

    /* transport ALPN */
    xqc_app_proto_callbacks_t transport_cbs = {{NULL}, {NULL}};
    xqc_engine_register_alpn(engine, "transport", 9, &transport_cbs);

    return engine;
}

const xqc_cid_t *
test_cid_connect(xqc_engine_t *engine)
{
    xqc_conn_settings_t conn_settings;
    memset(&conn_settings, 0, sizeof(xqc_conn_settings_t));
    conn_settings.proto_version = XQC_VERSION_V1;
    
    xqc_conn_ssl_config_t conn_ssl_config;
    memset(&conn_ssl_config, 0, sizeof(conn_ssl_config));
    const xqc_cid_t *cid = xqc_connect(engine, &conn_settings, NULL, 0, "", 0, &conn_ssl_config,
                                       NULL, 0, "transport", NULL);
    return cid;
}

static xqc_connection_t *
test_connect(xqc_engine_t *engine)
{
    const xqc_cid_t *cid = test_cid_connect(engine);
    if (cid == NULL) {
        return NULL;
    }
    return xqc_engine_conns_hash_find(engine, cid, 's');
}

xqc_connection_t *
test_engine_connect()
{
    xqc_engine_t *engine = test_create_engine();
    if (engine == NULL) {
        return NULL;
    }
    xqc_connection_t *conn = test_connect(engine);
    return conn;
}

