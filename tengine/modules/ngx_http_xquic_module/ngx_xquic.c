/*
 * Copyright (C) 2020-2023 Alibaba Group Holding Limited
 */

/**
 * for engine and socket operation
 */

#include <ngx_xquic.h>
#include <ngx_http_xquic_module.h>
#include <ngx_http_xquic.h>
#include <ngx_xquic_intercom.h>
#include <ngx_xquic_recv.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <xquic/xquic_typedef.h>
#include <xquic/xquic.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#define NGX_XQUIC_TMP_BUF_LEN 512
#define NGX_XQUIC_SUPPORT_CID_ROUTE 1
#if (T_NGX_UDPV2)
static void ngx_xquic_batch_udp_traffic(ngx_event_t *ev);
#endif

// 定义结构体类型 xqc_engine_callback_t
xqc_engine_callback_t ngx_xquic_engine_callback = {

// 预处理指令
#if (NGX_XQUIC_SUPPORT_CID_ROUTE) // 若NGX_XQUIC_SUPPORT_CID_ROUTE被宏定义
    .cid_generate_cb = ngx_xquic_cid_generate_cb,
#endif
    // 公共成员，无论条件如何都包含 set_event_timer 成员
    .set_event_timer = ngx_xquic_engine_set_event_timer,
    // 包含一个嵌套的结构体 log_callbacks
    .log_callbacks = {
        .xqc_log_write_err = ngx_xquic_log_write_err,
        .xqc_log_write_stat = ngx_xquic_log_write_stat,
    },
    .get_mp_cb = xqc_server_get_mp_by_addr,
    .load_CCA_cb = xqc_server_load_CCA_param,
    .save_CCA_cb = xqc_server_save_CCA_param,
};

xqc_transport_callbacks_t ngx_xquic_transport_callbacks = {

    .server_accept = ngx_xquic_conn_accept,
    .server_refuse = ngx_xquic_conn_refuse,
    .write_socket = ngx_xquic_server_send,
#if defined(T_NGX_XQUIC_SUPPORT_SENDMMSG)
    .write_mmsg = ngx_xquic_server_send_mmsg,
#endif
    .conn_update_cid_notify = ngx_http_v3_conn_update_cid_notify,
    .conn_cert_cb = ngx_http_v3_cert_cb,
};

/**
 * @brief from this line, functions below are added by jndu for CCA switching
 */
char g_local_addr_str[INET6_ADDRSTRLEN];
char g_peer_addr_str[INET6_ADDRSTRLEN];

char *
xqc_local_addr2str(const struct sockaddr *local_addr, socklen_t local_addrlen)
{
    if (local_addrlen == 0 || local_addr == NULL)
    {
        g_local_addr_str[0] = '\0';
        return g_local_addr_str;
    }

    struct sockaddr_in *sa_local = (struct sockaddr_in *)local_addr;
    if (sa_local->sin_family == AF_INET)
    {
        if (inet_ntop(sa_local->sin_family, &sa_local->sin_addr, g_local_addr_str, local_addrlen) == NULL)
        {
            g_local_addr_str[0] = '\0';
        }
    }
    else
    {
        if (inet_ntop(sa_local->sin_family, &((struct sockaddr_in6 *)sa_local)->sin6_addr,
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
    if (peer_addrlen == 0 || peer_addr == NULL)
    {
        g_peer_addr_str[0] = '\0';
        return g_peer_addr_str;
    }

    struct sockaddr_in *sa_peer = (struct sockaddr_in *)peer_addr;
    if (sa_peer->sin_family == AF_INET)
    {
        if (inet_ntop(sa_peer->sin_family, &sa_peer->sin_addr, g_peer_addr_str, peer_addrlen) == NULL)
        {
            g_peer_addr_str[0] = '\0';
        }
    }
    else
    {
        if (inet_ntop(sa_peer->sin_family, &((struct sockaddr_in6 *)sa_peer)->sin6_addr,
                      g_peer_addr_str, peer_addrlen) == NULL)
        {
            g_peer_addr_str[0] = '\0';
        }
    }

    return g_peer_addr_str;
}

void *xqc_server_get_mp_by_addr(xqc_CCA_info_container_t *container, const struct sockaddr *sa_peer, socklen_t peer_addrlen)
{
    char addr_str[2 * (INET6_ADDRSTRLEN) + 10];
    memset(addr_str, 0, sizeof(addr_str));
    size_t addr_len = snprintf(addr_str, sizeof(addr_str), "l-%s-%d p-%s-%d",
                               "127.0.0.1", 10,
                               xqc_peer_addr2str((struct sockaddr *)sa_peer, peer_addrlen),
                               10);
    xqc_ip_CCA_info_t **mp_map = xqc_CCA_info_container_find(container, addr_str, addr_len);
    if (!mp_map)
    {
        mp_map = malloc(sizeof(xqc_ip_CCA_info_t *) * XQC_MAX_PATHS_COUNT);
        memset(mp_map, 0, sizeof(xqc_ip_CCA_info_t *) * XQC_MAX_PATHS_COUNT);
        if (xqc_CCA_info_container_add(container,
                                       addr_str, addr_len, mp_map))
        {
            free(mp_map);
            mp_map = NULL;
        }
    }
    return mp_map;
}

const xmlChar *IPCONF_ = (const xmlChar *)"ip";
const xmlChar *CONF_ = (const xmlChar *)"conf";
const xmlChar *ADDR = (const xmlChar *)"addr";
const xmlChar *INTERFACE = (const xmlChar *)"interface";
const xmlChar *IF = (const xmlChar *)"if";
const xmlChar *CCA = (const xmlChar *)"CCA";
const xmlChar *ID = (const xmlChar *)"id";
const xmlChar *CNT = (const xmlChar *)"cnt";

xqc_int_t
xqc_server_load_CCA_info(xmlDocPtr doc, xmlNodePtr cur, xqc_ip_CCA_info_t *CCA_info)
{
    memset(CCA_info, 0, sizeof(xqc_ip_CCA_info_t) * XQC_CCA_NUM);
    xmlNodePtr cur_child = cur->children;
    xmlChar *id;

    if (cur == NULL)
    {
        return -1;
    }

    /* traverse CCA info */
    while (cur_child != NULL)
    {
        if (xmlStrcmp(cur_child->name, CCA) == 0)
        {
            id = xmlGetProp(cur_child, ID);
            int id_i = atoi((char *)id);
            xqc_ip_CCA_info_t *info = CCA_info + id_i;

            /* put nums from xml file to CCA_info */
            xmlNodePtr child_child = cur_child->children;
            while (child_child != NULL)
            {
                // char* endptr;
                if (!xmlStrcmp(child_child->name, CNT))
                {
                    // endptr = (char*)xmlNodeListGetString(doc, child_child->children, 0);
                    info->CCA_cnt = atoi((const char *)xmlNodeListGetString(doc, child_child->children, 0));
                }
                child_child = child_child->next;
            }
        }
        cur_child = cur_child->next;
    }

    return 0;
}

xqc_int_t
xqc_server_load_CCA_param(xqc_CCA_info_container_t *container, const char *file)
{
    xmlDocPtr doc;
    xmlNodePtr cur;
    /* for 1-level node, parsing the ip, port and interface. */
    xmlChar *addr;
    // xmlChar *if_;
    /* for 2-level node, parsing the different CCA. */
    // xmlChar *id;

    /* get the tree. */
    doc = xmlReadFile(file, NULL, 0);
    if (doc == NULL)
    {
        goto FAILED;
    }

    /* get the root. */
    cur = xmlDocGetRootElement(doc);
    if (cur == NULL)
    {
        goto FAILED;
    }

    if (xmlStrcmp(cur->name, IPCONF_) != 0)
    {
        goto FAILED;
    }

    /* traverse all the node. */
    cur = cur->children;
    /* traverse by ip+port */
    while (cur != NULL)
    {
        if (xmlStrcmp(cur->name, CONF_) == 0)
        {
            addr = xmlGetProp(cur, ADDR);

            xmlNodePtr cur_child = cur->children;
            /* traverse by interface and add mp_map to conn_CCA_hash */
            xqc_ip_CCA_info_t **mp_map = malloc(sizeof(xqc_ip_CCA_info_t *) * XQC_MAX_PATHS_COUNT);
            memset(mp_map, 0, sizeof(xqc_ip_CCA_info_t *) * XQC_MAX_PATHS_COUNT);
            while (cur_child != NULL)
            {
                if (xmlStrcmp(cur_child->name, INTERFACE) == 0)
                {
                    // if_ = xmlGetProp(cur_child, IF);
                    // assume that index is zero
                    int index = 0;
                    if (index >= 0)
                    {
                        xqc_ip_CCA_info_t *CCA_info = malloc(sizeof(xqc_ip_CCA_info_t) * XQC_CCA_NUM);
                        xqc_server_load_CCA_info(doc, cur_child, CCA_info);
                        mp_map[index] = CCA_info;
                    }
                }
                cur_child = cur_child->next;
            }

            /* add mp_hash to conn_CCA_hash */
            if (xqc_CCA_info_container_add(container, (char *)addr, strlen((char *)addr), mp_map))
            {
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
xqc_server_save_CCA_info(xmlNodePtr ip_node, xqc_ip_CCA_info_t **mp_map)
{
    xqc_ip_CCA_info_t *CCA_info = mp_map[0];

    if (CCA_info)
    {
        /* add interface node */
        xmlNodePtr if_node = xmlNewNode(NULL, INTERFACE);
        if (if_node == NULL)
        {
            return -1;
        }

        xmlNewProp(if_node, IF, INTERFACE);

        /* add CCA_info to interface node */
        for (int j = 0; j < XQC_CCA_NUM; j++)
        {
            char content[10];
            memset(content, 0, 10);

            xmlNodePtr CCA_node = xmlNewNode(NULL, CCA);
            sprintf(content, "%d", j);
            xmlNewProp(CCA_node, ID, (const xmlChar *)content);
            if (CCA_node == NULL)
            {
                goto FAILED;
            }

            memset(content, 0, 10);
            sprintf(content, "%d", CCA_info[j].CCA_cnt);
            if (xmlNewChild(CCA_node, NULL, CNT, (const xmlChar *)content) == NULL)
            {
                goto FAILED;
            }

            xmlAddChild(if_node, CCA_node);
        }

        xmlAddChild(ip_node, if_node);
    }

    return 0;

FAILED:
    return -1;
}

xqc_int_t
xqc_server_save_CCA_param(xqc_CCA_info_container_t *container, const char *file)
{
    xmlDocPtr doc = NULL;
    xmlNodePtr root = NULL;

    /* create a new xml file */
    doc = xmlNewDoc(BAD_CAST "1.0");
    if (doc == NULL)
    {
        goto FAILED;
    }

    /* create a root node */
    root = xmlNewNode(NULL, IPCONF_);
    if (root == NULL)
    {
        goto FAILED;
    }

    /* add root to doc */
    xmlDocSetRootElement(doc, root);

    /* traverse conn_CCA_hash */
    xqc_ip_CCA_info_t **mp_map;
    char addr[2 * (INET6_ADDRSTRLEN) + 10];
    size_t addr_len;
    while ((mp_map = xqc_CCA_info_container_get(container, addr, &addr_len)))
    {
        /* add ip+port node */
        xmlNodePtr ip_node = xmlNewNode(NULL, CONF_);
        if (ip_node == NULL)
        {
            goto FAILED;
        }

        xmlNewProp(ip_node, ADDR, (const xmlChar *)addr);

        /* add interface node */
        if (xqc_server_save_CCA_info(ip_node, mp_map))
        {
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
    if (doc)
    {
        xmlFreeDoc(doc);
    }

    return -1;
}

float xqc_CCA_switching_get_metric(xqc_stream_CCA_info_t *CCA_sampler, xqc_ip_CCA_info_t *CCA_info, int index)
{
    // get power1 from throughput
    uint64_t throughput = xqc_get_CCA_info_sample(CCA_sampler, XQC_CCA_INFO_SAMPLE_THROUGHPUT);
    uint64_t max_throughput = xqc_get_CCA_info_sample(CCA_sampler, XQC_CCA_INFO_SAMPLE_MAX_THROUGHPUT);
    float power1, power2, power3, power4;
    if (max_throughput)
    {
        power1 = (throughput * 1.0) / (max_throughput * 1.0);
    }
    else
    {
        power1 = 0;
    }
    // get power2 from loss_rate
    uint64_t loss_rate = xqc_get_CCA_info_sample(CCA_sampler, XQC_CCA_INFO_SAMPLE_LOSS_RATE);
    power2 = 1 - (loss_rate * 1.0 / 1000);
    // get power3 from rtt
    uint64_t latest_rtt = xqc_get_CCA_info_sample(CCA_sampler, XQC_CCA_INFO_SAMPLE_LATEST_RTT);
    uint64_t min_rtt = xqc_get_CCA_info_sample(CCA_sampler, XQC_CCA_INFO_SAMPLE_MIN_RTT);
    if (min_rtt)
    {
        power3 = (min_rtt * 1.0) / (latest_rtt * 1.0);
    }
    else
    {
        power3 = 0;
    }
    // get power4 from ip info
    uint64_t CCA_cnt_sum = 0;
    for (size_t i = 0; i < XQC_CCA_NUM; i++)
    {
        CCA_cnt_sum += CCA_info[i].CCA_cnt;
    }
    if (CCA_cnt_sum)
    {
        power4 = (CCA_info[index].CCA_cnt * 1.0) / (CCA_cnt_sum * 1.0);
    }
    else
    {
        power4 = 0;
    }
    // get metric
    return 0 * power4 + 1 * power1 + 0 * power2 + 0 * power3;
}
/**
 * @brief at this line, upper functions are added by jndu for CCA switching
 */

uint64_t
ngx_xquic_get_time() // 获取当前时间的微秒级别的时间戳
{
    /* take the time in microseconds */
    // 获取当前时间（精确到微秒）
    struct timeval tv;
    gettimeofday(&tv, NULL);
    // 计算微秒级别的时间戳
    uint64_t ul = tv.tv_sec * 1000000 + tv.tv_usec;
    return ul;
}

/* TODO: close file */
ngx_int_t
ngx_xquic_read_file_data(char *data, size_t data_len, char *filename) // 用于从文件中读取数据到指定的缓冲区
{
    FILE *fp = fopen(filename, "rb");

    if (fp == NULL)
    {
        return -1;
    }
    // 定位到文件末尾以获取文件总长度
    fseek(fp, 0, SEEK_END);
    size_t total_len = ftell(fp);
    // 将文件指针重新定位到文件开头
    fseek(fp, 0, SEEK_SET);
    // 如果文件长度大于目标数据缓冲区长度，则返回错误
    if (total_len > data_len)
    {
        return -1;
    }
    // 读取文件数据到指定的缓冲区
    size_t read_len = fread(data, 1, total_len, fp);
    // 检查读取是否成功
    if (read_len != total_len)
    {

        return -1;
    }

    return read_len;
}

/* run main logic */
void ngx_xquic_engine_timer_callback(ngx_event_t *ev)
{
    // 将事件结构体的 data 指针强制转换为 xqc_engine_t 类型的指针
    xqc_engine_t *engine = (xqc_engine_t *)(ev->data);
    // 调用 xqc_engine_main_logic 函数，传递 engine 指针作为参数
    xqc_engine_main_logic(engine);
    return;
}

void ngx_xquic_engine_init_event_timer(ngx_http_xquic_main_conf_t *qmcf, xqc_engine_t *engine)
{
    // 获取引擎的定时器事件结构体
    ngx_event_t *ev = &(qmcf->engine_ev_timer);
    // 打印调试信息，输出引擎的地址
    ngx_log_error(NGX_LOG_DEBUG, ngx_cycle->log, 0,
                  "|xquic|ngx_xquic_init_event_timer|%p|", engine);

    // 将定时器事件结构体清零
    ngx_memzero(ev, sizeof(ngx_event_t));
    // 设置定时器事件的回调函数
    ev->handler = ngx_xquic_engine_timer_callback;
    // 设置日志对象
    ev->log = ngx_cycle->log;
    // 将引擎对象作为数据附加到定时器事件上
    ev->data = engine;
// 如果启用了 NGX_UDPV2，则复制事件结构体以备后用，并设置对应的回调函数
#if (T_NGX_UDPV2)
    ngx_memcpy(&qmcf->udpv2_batch, ev, sizeof(*ev));
    qmcf->udpv2_batch.handler = ngx_xquic_batch_udp_traffic;
#endif
}

void ngx_xquic_engine_set_event_timer(xqc_msec_t wake_after, void *engine_user_data)
{
    // 将引擎用户数据转换为 ngx_http_xquic_main_conf_t 类型
    ngx_http_xquic_main_conf_t *qmcf = (ngx_http_xquic_main_conf_t *)engine_user_data;
    // 将触发时间转换为毫秒
    ngx_msec_t wake_after_ms = wake_after / 1000;
    // 如果触发时间为零，将其设置为最小的定时器间隔（1毫秒）
    if (wake_after_ms == 0)
    {
        wake_after_ms = 1; // most event timer interval 1
    }
    // 如果定时器已设置，则先删除之前的定时器
    if (qmcf->engine_ev_timer.timer_set)
    {
        ngx_del_timer(&(qmcf->engine_ev_timer));
    }
    // 添加新的定时器，触发时间为 wake_after_ms 毫秒后
    ngx_add_timer(&(qmcf->engine_ev_timer), wake_after_ms);
}

ngx_int_t
ngx_xquic_engine_init_alpn_ctx(ngx_cycle_t *cycle, xqc_engine_t *engine)
{
    ngx_int_t ret = NGX_OK;

    // 定义 HTTP/3 回调函数结构体
    xqc_h3_callbacks_t h3_cbs = {
        .h3c_cbs = {
            .h3_conn_create_notify = ngx_http_v3_conn_create_notify,
            .h3_conn_close_notify = ngx_http_v3_conn_close_notify,
            .h3_conn_handshake_finished = ngx_http_v3_conn_handshake_finished,
        },
        .h3r_cbs = {
            .h3_request_write_notify = ngx_http_v3_request_write_notify,
            .h3_request_read_notify = ngx_http_v3_request_read_notify,
            .h3_request_create_notify = ngx_http_v3_request_create_notify,
            .h3_request_close_notify = ngx_http_v3_request_close_notify,
        }};

    /* init http3 context */
    // 初始化 HTTP/3 上下文
    ret = xqc_h3_ctx_init(engine, &h3_cbs);
    if (ret != XQC_OK)
    {
        ngx_log_error(NGX_LOG_EMERG, cycle->log, 0, "init h3 context error, ret: %d\n", ret);
        return ret;
    }

    return ret;
}

ngx_int_t
ngx_xquic_engine_init(ngx_cycle_t *cycle)
{
    // 获取 ngx_http_xquic 模块的主配置
    ngx_http_xquic_main_conf_t *qmcf = ngx_http_cycle_get_module_main_conf(cycle, ngx_http_xquic_module);
    // 获取默认的引擎配置
    xqc_engine_ssl_config_t *engine_ssl_config = NULL;
    xqc_config_t config;

    // 如果无法获取默认引擎配置，返回 NGX_ERROR
    if (xqc_engine_get_default_config(&config, XQC_ENGINE_SERVER) < 0)
    {
        return NGX_ERROR;
    }

    // 如果配置了状态重置令牌密钥，将其拷贝到引擎配置中
    if (qmcf->stateless_reset_token_key.len > 0 && qmcf->stateless_reset_token_key.len <= XQC_RESET_TOKEN_MAX_KEY_LEN)
    {
        strncpy(config.reset_token_key, (char *)qmcf->stateless_reset_token_key.data, XQC_RESET_TOKEN_MAX_KEY_LEN);
        config.reset_token_keylen = qmcf->stateless_reset_token_key.len;
    }

    // 检查是否成功获取了主配置
    if (qmcf == NULL)
    {
        ngx_log_error(NGX_LOG_EMERG, cycle->log, 0,
                      "|xquic|ngx_xquic_engine_init: get main conf fail|");
        return NGX_ERROR;
    }

    // 如果引擎已经初始化，直接返回 NGX_OK
    if (qmcf->xquic_engine != NULL)
    {
        return NGX_OK;
    }

    /* enable cid negotiate */
    // 启用cid协商
#if (NGX_XQUIC_SUPPORT_CID_ROUTE)
    // 如果编译时启用了 CID 路由支持
    if (ngx_xquic_is_cid_route_on(cycle))
    {
        // 设置引擎配置中的 CID 协商标志为1（启用）
        config.cid_negotiate = 1;
        // 设置引擎配置中的 CID 长度为主配置中的 CID 长度
        config.cid_len = qmcf->cid_len;
        /* using time and pid as the seed for a new sequence of pseudo-random integer */
        // 使用当前时间和进程ID作为伪随机整数序列的种子
        srandom(time(NULL) + getpid());
    }
#endif

    /* init log level */
    // 初始化日志级别
    config.cfg_log_level = qmcf->log_level;
    config.cfg_log_timestamp = 0;

// 如果定义了 T_NGX_XQUIC_SUPPORT_SENDMMSG，则启用 sendmmsg
#if defined(T_NGX_XQUIC_SUPPORT_SENDMMSG)
    /* set sendmmsg */
    config.sendmmsg_on = 1;
#endif

    /* init ssl config */
    // 初始化 SSL 配置
    engine_ssl_config = &(qmcf->engine_ssl_config);

    // 检查是否提供了有效的证书和私钥
    if (qmcf->certificate.len == 0 || qmcf->certificate.data == NULL || qmcf->certificate_key.len == 0 || qmcf->certificate_key.data == NULL)
    {
        ngx_log_error(NGX_LOG_EMERG, cycle->log, 0,
                      "|xquic|ngx_xquic_engine_init: null certificate or key|");
        return NGX_ERROR;
    }

    /* copy cert key */
    // 复制私钥文件路径
    engine_ssl_config->private_key_file = ngx_pcalloc(cycle->pool, qmcf->certificate_key.len + 1);
    if (engine_ssl_config->private_key_file == NULL)
    {
        ngx_log_error(NGX_LOG_EMERG, cycle->log, 0,
                      "|xquic|ngx_xquic_engine_init: fail to alloc memory|");
        return NGX_ERROR;
    }
    ngx_memcpy(engine_ssl_config->private_key_file, qmcf->certificate_key.data, qmcf->certificate_key.len);

    /* copy cert */
    // 复制证书文件路径
    engine_ssl_config->cert_file = ngx_pcalloc(cycle->pool, qmcf->certificate.len + 1);
    if (engine_ssl_config->cert_file == NULL)
    {
        ngx_log_error(NGX_LOG_EMERG, cycle->log, 0,
                      "|xquic|ngx_xquic_engine_init: fail to alloc memory|");
        return NGX_ERROR;
    }
    ngx_memcpy(engine_ssl_config->cert_file, qmcf->certificate.data, qmcf->certificate.len);

    engine_ssl_config->ciphers = XQC_TLS_CIPHERS;
    engine_ssl_config->groups = XQC_TLS_GROUPS;

    /* copy session ticket */
    // 复制会话ticket
    char g_ticket_file[NGX_XQUIC_TMP_BUF_LEN] = {0};
    char g_session_ticket_key[NGX_XQUIC_TMP_BUF_LEN];
    if (qmcf->session_ticket_key.data != NULL && qmcf->session_ticket_key.len != 0 && qmcf->session_ticket_key.len < NGX_XQUIC_TMP_BUF_LEN)
    {
        // 将主配置中的会话票据密钥路径复制到临时缓冲区中
        ngx_memcpy(g_ticket_file, qmcf->session_ticket_key.data, qmcf->session_ticket_key.len);

        // 读取会话票据密钥文件中的数据
        int ticket_key_len = ngx_xquic_read_file_data(g_session_ticket_key,
                                                      sizeof(g_session_ticket_key),
                                                      g_ticket_file);
        // 打印调试信息，输出读取的会话票据密钥长度
        ngx_log_error(NGX_LOG_DEBUG, cycle->log, 0,
                      "|xquic|ngx_xquic_engine_init: ticket_key_len=%i|", ticket_key_len);

        // 如果读取失败，设置引擎的会话票据密钥数据为NULL
        if (ticket_key_len < 0)
        {
            engine_ssl_config->session_ticket_key_data = NULL;
            engine_ssl_config->session_ticket_key_len = 0;
        }
        else
        {
            // 为引擎的会话票据密钥数据分配内存
            engine_ssl_config->session_ticket_key_data = ngx_pcalloc(cycle->pool, (size_t)ticket_key_len);
            if (engine_ssl_config->session_ticket_key_data == NULL)
            {
                ngx_log_error(NGX_LOG_EMERG, cycle->log, 0,
                              "|xquic|ngx_xquic_engine_init: fail to alloc memory|");
                return NGX_ERROR;
            }
            // 设置引擎的会话票据密钥长度和数据
            engine_ssl_config->session_ticket_key_len = ticket_key_len;
            ngx_memcpy(engine_ssl_config->session_ticket_key_data, g_session_ticket_key, ticket_key_len);
        }
    }

    /* create engine */
    // 创建引擎
    qmcf->xquic_engine = xqc_engine_create(XQC_ENGINE_SERVER, &config, engine_ssl_config,
                                           &ngx_xquic_engine_callback, &ngx_xquic_transport_callbacks, qmcf);
    if (qmcf->xquic_engine == NULL)
    {
        ngx_log_error(NGX_LOG_EMERG, cycle->log, 0,
                      "|xquic|xqc_engine_create: fail|");
        return NGX_ERROR;
    }

    /* init http3 alpn context */
    // 初始化 HTTP/3 ALPN 上下文
    if (ngx_xquic_engine_init_alpn_ctx(cycle, qmcf->xquic_engine) != NGX_OK)
    {
        return NGX_ERROR;
    }

    /* set congestion control */
    // 设置拥塞控制
    xqc_cong_ctrl_callback_t cong_ctrl; // hook
    if (qmcf->congestion_control.len == sizeof("bbr") - 1 && ngx_strncmp(qmcf->congestion_control.data, "bbr", sizeof("bbr") - 1) == 0)
    {
        cong_ctrl = xqc_bbr_cb;
    }
    else if (qmcf->congestion_control.len == sizeof("reno") - 1 && ngx_strncmp(qmcf->congestion_control.data, "reno", sizeof("reno") - 1) == 0)
    {
        cong_ctrl = xqc_reno_cb;
    }
    else if (qmcf->congestion_control.len == sizeof("copa") - 1 && ngx_strncmp(qmcf->congestion_control.data, "copa", sizeof("copa") - 1) == 0)
    {
        cong_ctrl = xqc_copa_cb; // copa加入
    }
    else if (qmcf->congestion_control.len == sizeof("cubic") - 1 && ngx_strncmp(qmcf->congestion_control.data, "cubic", sizeof("cubic") - 1) == 0)
    {
        cong_ctrl = xqc_cubic_cb;
    }
    else
    {
        ngx_log_error(NGX_LOG_EMERG, cycle->log, 0,
                      "|xquic|unknown xquic_congestion_control|%V|", &qmcf->congestion_control);
        return NGX_ERROR;
    }

    // 根据配置设置 pacing_on
    int pacing_on = (qmcf->pacing_on ? 1 : 0);

    // 初始化连接参数结构体
    xqc_conn_settings_t conn_settings = {
        .pacing_on = pacing_on,
        .cong_ctrl_callback = cong_ctrl,
        .metric_cb = xqc_CCA_switching_get_metric,
        conn_settings.expected_time = 5,
    };
    // added by qnwang for AR
    if (qmcf->ditto_expected_time != NGX_CONF_UNSET_UINT)
    {
        conn_settings.expected_time = (xqc_msec_t)qmcf->ditto_expected_time;
    }

    // 如果配置了 anti_amplification_limit，设置到连接参数中
    if (qmcf->anti_amplification_limit != NGX_CONF_UNSET_UINT)
    {
        conn_settings.anti_amplification_limit = qmcf->anti_amplification_limit;
    }

    // 如果配置了 keyupdate_pkt_threshold，设置到连接参数中
    if (qmcf->keyupdate_pkt_threshold != NGX_CONF_UNSET_UINT)
    {
        conn_settings.keyupdate_pkt_threshold = qmcf->keyupdate_pkt_threshold;
    }

    // 设置连接参数
    xqc_server_set_conn_settings(&conn_settings);

    // 设置 QPACK 编码器和解码器的动态表容量
    xqc_h3_engine_set_enc_max_dtable_capacity(qmcf->xquic_engine,
                                              qmcf->qpack_encoder_dynamic_table_capacity);
    xqc_h3_engine_set_dec_max_dtable_capacity(qmcf->xquic_engine,
                                              qmcf->qpack_decoder_dynamic_table_capacity);

    /* init event timer */
    // 初始化事件定时器
    ngx_xquic_engine_init_event_timer(qmcf, qmcf->xquic_engine);

    // 记录调试级别的日志，输出引擎的地址
    ngx_log_error(NGX_LOG_DEBUG, cycle->log, 0,
                  "|xquic|xquic_engine|%p|", qmcf->xquic_engine);

    return NGX_OK;
}

#if (T_NGX_UDPV2)

static void
ngx_xquic_batch_udp_traffic(ngx_event_t *ev)
{
    xqc_engine_t *xquic_engine;
    xquic_engine = (xqc_engine_t *)(ev->data);
    xqc_engine_finish_recv(xquic_engine);
    ngx_accept_disabled = ngx_cycle->connection_n / 8 - ngx_cycle->free_connection_n;
}

static ngx_udpv2_traffic_filter_retcode
ngx_xquic_udp_accept_filter(ngx_listening_t *ls, const ngx_udpv2_packet_t *upkt)
{

    ngx_connection_t *lc;
    ngx_http_xquic_main_conf_t *qmcf;

    lc = ls->connection;
    qmcf = (ngx_http_xquic_main_conf_t *)(lc->data);

    // feed to xquic
    ngx_xquic_dispatcher_process(lc, upkt);
    // posted udpv2 event
    ngx_post_event(&qmcf->udpv2_batch, &ngx_udpv2_posted_event);

    return NGX_UDPV2_DONE;
}
#endif

ngx_int_t
ngx_xquic_process_init(ngx_cycle_t *cycle)
{
    int with_xquic = 0;
    ngx_http_xquic_main_conf_t *qmcf = ngx_http_cycle_get_module_main_conf(cycle, ngx_http_xquic_module);

    // 记录调试级别的日志，表示 ngx_xquic_process_init 函数开始执行
    ngx_log_debug0(NGX_LOG_DEBUG_CORE, cycle->log, 0, "|xquic|ngx_xquic_process_init|");

    /* socket init */
    // 初始化 socket
    ngx_event_t *rev;
    ngx_listening_t *ls;
    ngx_connection_t *c;
    unsigned int i;

    // 循环遍历监听列表
    ls = (ngx_listening_t *)(cycle->listening.elts);
    for (i = 0; i < cycle->listening.nelts; i++)
    {

#if !(T_RELOAD)
#if (NGX_HAVE_REUSEPORT)
        // 如果启用了 REUSEPORT 且当前监听端口是 REUSEPORT 的子监听端口，则跳过
        if (ls[i].reuseport && ls[i].worker != ngx_worker)
        {
            continue;
        }
#endif
#endif

        // 如果当前监听端口的文件描述符为 -1，说明未成功打开，则跳过
        if (ls[i].fd == -1)
        {
            continue;
        }

        // 如果当前监听端口不是 xquic 类型的监听，则跳过
        if (!ls[i].xquic)
        {
            continue;
        }

        // 设置 with_xquic 为 1，表示存在 xquic 类型的监听端口
        with_xquic = 1;

        // 获取当前监听端口的连接和读事件
        c = ls[i].connection;
        rev = c->read;

#if (T_NGX_UDPV2)
        /* outofband */
        if (ls[i].support_udpv2)
        {
            ngx_udpv2_reset_dispatch_filter(&ls[i]);
            ngx_udpv2_push_dispatch_filter(cycle, &ls[i], ngx_xquic_udp_accept_filter);
        }
#endif

        // 设置读事件的处理函数为 ngx_xquic_event_recv
        rev->handler = ngx_xquic_event_recv;
        // 将连接的 data 字段设置为指向 ngx_http_xquic_main_conf_t 结构体的指针
        c->data = qmcf;
        // 检查 data 是否为空，如果为空则记录错误日志并返回 NGX_ERROR
        if (c->data == NULL)
        {
            ngx_log_error(NGX_LOG_EMERG, cycle->log, 0,
                          "|xquic|ngx_xquic_process_init|qmcf equals NULL|");
            return NGX_ERROR;
        }
    }

    /* socket init end */
    if (with_xquic && ngx_xquic_engine_init(cycle) != NGX_OK)
    {
        // 如果引擎初始化失败，记录错误日志并返回 NGX_ERROR
        ngx_log_error(NGX_LOG_EMERG, cycle->log, 0,
                      "|xquic|ngx_xquic_process_init|engine_init fail|");
        return NGX_ERROR;
    }

    // 如果存在 xquic 类型的监听端口，调用 ngx_xquic_intercom_init 初始化 intercom（通信模块）
    if (with_xquic && ngx_xquic_intercom_init(cycle, qmcf->xquic_engine) != NGX_OK)
    {
        // 如果 intercom 初始化失败，返回 NGX_ERROR
        return NGX_ERROR;
    }

    return NGX_OK;
}

void ngx_xquic_process_exit(ngx_cycle_t *cycle)
{
    // 获取 ngx_http_xquic 模块的主配置
    ngx_http_xquic_main_conf_t *qmcf = ngx_http_cycle_get_module_main_conf(cycle, ngx_http_xquic_module);

    // 记录调试级别的日志，表示 ngx_xquic_process_exit 函数开始执行
    ngx_log_debug0(NGX_LOG_DEBUG_CORE, cycle->log, 0, "|xquic|ngx_xquic_process_exit|");

    // 如果 xquic 引擎存在，则销毁引擎并执行 intercom 模块的退出操作
    if (qmcf->xquic_engine)
    {
        xqc_engine_destroy(qmcf->xquic_engine);
        qmcf->xquic_engine = NULL;

        ngx_xquic_intercom_exit();
    }
}

void ngx_xquic_log_write_err(xqc_log_level_t lvl, const void *buf, size_t size, void *engine_user_data)
{
    ngx_log_error(NGX_LOG_WARN, ngx_cycle->log, 0, "|xquic|lib%*s|", size, buf);
}

void ngx_xquic_log_write_stat(xqc_log_level_t lvl, const void *buf, size_t size, void *engine_user_data)
{
    ngx_log_xquic(NGX_LOG_WARN, ngx_cycle->x_log, 0, "%*s|", size, buf);
}

#if (NGX_XQUIC_SUPPORT_CID_ROUTE)

ngx_int_t
ngx_xquic_is_cid_route_on(ngx_cycle_t *cycle)
{
    ngx_http_xquic_main_conf_t *qmcf = ngx_http_cycle_get_module_main_conf(cycle, ngx_http_xquic_module);
    return !!qmcf->cid_route;
}

ngx_inline ngx_int_t
ngx_xquic_init_cid_route(ngx_cycle_t *cycle, ngx_http_xquic_main_conf_t *qmcf)
{
    ngx_cycle_t *old_cycle;
    ngx_http_xquic_main_conf_t *old_qmcf;

    // 如果主配置为空，返回 NGX_ERROR
    if (!qmcf)
    {
        return NGX_ERROR;
    }

    // 获取旧的 ngx_cycle_t 和 ngx_http_xquic_main_conf_t
    old_cycle = cycle->old_cycle;
    old_qmcf = (old_cycle && !ngx_is_init_cycle(old_cycle)) ? ngx_http_cycle_get_module_main_conf(old_cycle, ngx_http_xquic_module) : NULL;

    /* set salt range */
    // 设置 CID Worker ID 的盐范围
    qmcf->cid_worker_id_salt_range = qmcf->cid_worker_id_offset;

    /* keep the same cid_worker_id_secret for the tengine reload */
    // 如果存在旧的配置，使用相同的 CID Worker ID 密钥
    if (old_qmcf)
    {
        /* use same cid_worker_id_secret */
        qmcf->cid_worker_id_secret = old_qmcf->cid_worker_id_secret;
    }
    else
    {
        // 如果不存在旧的配置，生成一个新的 CID Worker ID 密钥
        srandom(time(NULL));
        /* generate security stuff */
        qmcf->cid_worker_id_secret = random();
    }

    return NGX_OK;
}

ngx_int_t
ngx_xquic_enable_cid_route(ngx_cycle_t *cycle)
{
    ngx_http_xquic_main_conf_t *qmcf = ngx_http_cycle_get_module_main_conf(cycle, ngx_http_xquic_module);
    if (!qmcf)
    {
        return NGX_ERROR;
    }

    if (qmcf->cid_route == NGX_CONF_UNSET)
    {
        /* need init xquic module first */
        return NGX_ERROR;
    }
    else if (qmcf->cid_route)
    {
        /* already enable */
        return NGX_OK;
    }

    qmcf->cid_route = 1;
    return ngx_xquic_init_cid_route(cycle, qmcf);
}

ssize_t
ngx_xquic_cid_generate_cb(const xqc_cid_t *ori_cid, uint8_t *cid_buf, size_t cid_buflen, void *engine_user_data)
{
    (void)engine_user_data;

    size_t current_cid_buflen;
    const uint8_t *current_cid_buf;

    current_cid_buf = NULL;
    current_cid_buflen = 0;

    if (ori_cid)
    {
        current_cid_buf = ori_cid->cid_buf;
        current_cid_buflen = ori_cid->cid_len;
    }

    return ngx_xquic_generate_route_cid(cid_buf, cid_buflen, current_cid_buf, current_cid_buflen);
}

uint32_t
ngx_xquic_cid_length(ngx_cycle_t *cycle)
{
    ngx_http_xquic_main_conf_t *qmcf = ngx_http_cycle_get_module_main_conf(cycle, ngx_http_xquic_module);
    return qmcf->cid_len;
}

uint32_t
ngx_xquic_cid_worker_id_salt_range(ngx_cycle_t *cycle)
{
    ngx_http_xquic_main_conf_t *qmcf = ngx_http_cycle_get_module_main_conf(cycle, ngx_http_xquic_module);
    return qmcf->cid_worker_id_salt_range;
}

uint32_t
ngx_xquic_cid_worker_id_offset(ngx_cycle_t *cycle)
{
    ngx_http_xquic_main_conf_t *qmcf = ngx_http_cycle_get_module_main_conf(cycle, ngx_http_xquic_module);
    return qmcf->cid_worker_id_offset;
}

uint32_t
ngx_xquic_cid_worker_id_secret(ngx_cycle_t *cycle)
{
    ngx_http_xquic_main_conf_t *qmcf = ngx_http_cycle_get_module_main_conf(cycle, ngx_http_xquic_module);
    return qmcf->cid_worker_id_secret;
}

static inline void
ngx_xquic_random_buf(unsigned char *p, size_t len) // 生成指定长度的随机字节序列
{
    uint32_t r;
    while (len > sizeof(r))
    {
        r = random();
        ngx_memcpy(p, &r, sizeof(r));
        p += sizeof(r);
        len -= sizeof(r);
    }
    if (len > 0)
    {
        r = random();
        ngx_memcpy(p, &r, len);
    }
}

ssize_t
ngx_xquic_generate_route_cid(unsigned char *buf, size_t len, const uint8_t *current_cid_buf, size_t current_cid_buflen)
{
    ngx_core_conf_t *ccf;
    ngx_http_xquic_main_conf_t *qmcf;
    uint32_t worker, salt;
    int32_t delta;

    // 获取 ngx_core_module 和 ngx_http_xquic_module 的配置信息
    qmcf = ngx_http_cycle_get_module_main_conf(ngx_cycle, ngx_http_xquic_module);
    ccf = (ngx_core_conf_t *)ngx_get_conf(ngx_cycle->conf_ctx, ngx_core_module);

    // 如果 len 小于所需的 CID 长度，返回 0
    if (XQC_UNLIKELY(len < qmcf->cid_len))
    {
        /**
         * just return 0 to force xquic generate random cid
         * Notes: broke the DCID spec
         */
        ngx_log_error(NGX_LOG_WARN, ngx_cycle->log, 0, "|xquic|dismatch cid length %d (required %d)|", len, qmcf->cid_len);
        return 0;
    }

    /* fill with random data */
    // 用随机数据填充 buf
    ngx_xquic_random_buf(buf, qmcf->cid_len);

    /* calculate salt */
    // 计算 salt
    salt = ngx_murmur_hash2(buf, qmcf->cid_worker_id_salt_range);

    /**
     * required :
     * 1. pid < 2 ^ 22  (SYSTEM LIMITED)
     * 2. ngx_worker < 1024
     * */

    // 计算 worker delta
    delta = ngx_worker - salt % ccf->worker_processes;
    if (delta < 0)
    {
        delta += ccf->worker_processes;
    }

// 设置 worker delta 和 PID 到 worker 变量
#define PID_MAX_BIT 22
    /* set worker delta */
    worker = delta << PID_MAX_BIT;
    /* set PID */
    worker = worker | getpid();
    /* encrypt worker */
    worker = htonl((worker + salt) ^ qmcf->cid_worker_id_secret);
    /* set worker id */
    ngx_memcpy(buf + qmcf->cid_worker_id_offset, &worker, sizeof(worker));
    // 返回 CID 的长度
    return qmcf->cid_len;
}

#ifdef UINT64_MAX
static ngx_inline uint32_t
ngx_sum_complement(uint64_t a, uint64_t b, uint32_t c)
{
    return (a + b) % c;
}
#endif

ngx_int_t
ngx_xquic_get_target_worker_from_cid(ngx_xquic_recv_packet_t *packet)
{
    ngx_core_conf_t *ccf;
    ngx_http_xquic_main_conf_t *qmcf;
    uint32_t worker, salt;
    u_char *dcid;

    // 获取 ngx_core_module 和 ngx_http_xquic_module 的配置信息
    ccf = (ngx_core_conf_t *)ngx_get_conf(ngx_cycle->conf_ctx, ngx_core_module);
    qmcf = ngx_http_cycle_get_module_main_conf(ngx_cycle, ngx_http_xquic_module);
    dcid = packet->xquic.dcid.cid_buf;

    // 如果 CID 的长度大于等于所需的长度
    if (packet->xquic.dcid.cid_len >= qmcf->cid_len)
    {
        /* calculate salt */
        // 计算 salt
        salt = ngx_murmur_hash2(dcid, qmcf->cid_worker_id_salt_range);
        /* get cipher worker */
        // 获取 cipher worker
        memcpy(&worker, dcid + qmcf->cid_worker_id_offset, sizeof(worker));
        /* decrypt */
        // 解密
        worker = (ntohl(worker) ^ qmcf->cid_worker_id_secret) - salt;

#ifdef UINT64_MAX
        return ngx_sum_complement(worker >> PID_MAX_BIT, salt, ccf->worker_processes);
#else
        /**
         * For the mathematics, ((worker >> PID_MAX_BIT) + salt) % ccf->worker_processes is better
         * (worker >> PID_MAX_BIT) + salt may overflow in practice
         * */
        return ((worker >> PID_MAX_BIT) % ccf->worker_processes + salt % ccf->worker_processes) % ccf->worker_processes;
#endif

#undef PID_MAX_BIT
    }
    // 如果 CID 的长度小于所需的长度，使用 MurmurHash2 计算目标 worker 编号
    return ngx_murmur_hash2(dcid, packet->xquic.dcid.cid_len) % ccf->worker_processes;
}
#endif
