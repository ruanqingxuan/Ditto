// added by jndu

#ifndef _XQC_CCA_INFO_CONTAINER_INCLUDED_
#define _XQC_CCA_INFO_CONTAINER_INCLUDED_

#include <stdint.h>

#include <xquic/xquic.h>


typedef struct xqc_CCA_info_container_s {
    xqc_CCA_info_container_type_t type;
    void *raw;
} xqc_CCA_info_container_t;

int
xqc_CCA_info_container_init(xqc_CCA_info_container_t *container, xqc_CCA_info_container_type_t type);

void
xqc_CCA_info_container_release(xqc_CCA_info_container_t *container);

void *
xqc_CCA_info_container_find(xqc_CCA_info_container_t *container, char *addr, size_t len);

int
xqc_CCA_info_container_add(xqc_CCA_info_container_t *container, char *addr, size_t len, xqc_ip_CCA_info_t **CCA_info);

int
xqc_CCA_info_container_delete(xqc_CCA_info_container_t *container, char *addr, size_t len);

xqc_ip_CCA_info_t**
xqc_CCA_info_container_get(xqc_CCA_info_container_t *container, char *addr, size_t *len);

void
xqc_CCA_info_container_free_value(xqc_ip_CCA_info_t **mp_map);

#endif /*_XQC_CCA_INFO_CONTAINER_INCLUDED_*/
