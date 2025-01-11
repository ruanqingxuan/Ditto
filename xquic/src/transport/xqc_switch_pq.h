// added by jndu for CCA switching
#ifndef _XQC_SWITCH_PQ_H_INCLUDED_
#define _XQC_SWITCH_PQ_H_INCLUDED_

#include <xquic/xquic.h>
#include "src/common/xqc_extra_log.h"
#include "src/transport/xqc_conn.h"
#include "src/transport/xqc_switch.h"
#include "src/common/xqc_malloc.h"
#include "src/transport/xqc_extra_sample.h"
#include <memory.h>

typedef struct
{
    float metric;
    xqc_switch_CCA_t switch_CCA;
    xqc_usec_t last_use_time;
} xqc_switch_pq_elem_t;

typedef int (*xqc_switch_pq_compare_ptr)(float power_1, float power_2);

static inline int
xqc_switch_pq_default_cmp(float power_1, float power_2)
{
    return (power_1 < power_2) ? 1 : 0;
}

typedef struct xqc_switch_pq_s
{
    char *elements;                /* elements */
    size_t element_size;           /* memory size of element objects */
    size_t count;                  /* number of elements */
    size_t capacity;               /* element capacity */
    xqc_allocator_t a;             /* memory allocator */
    xqc_switch_pq_compare_ptr cmp; /* compare function */
    xqc_usec_t build_time;
} xqc_switch_pq_t;

#define xqc_switch_pq_element(pq, index) ((xqc_switch_pq_elem_t *)&(pq)->elements[(index) * (pq)->element_size])
#define xqc_switch_pq_element_copy(pq, dst, src) memmove(xqc_switch_pq_element((pq), (dst)), xqc_switch_pq_element((pq), (src)), (pq)->element_size)
#define xqc_switch_pq_default_capacity XQC_CCA_NUM

static inline int
xqc_switch_pq_init(xqc_switch_pq_t *pq, size_t capacity, xqc_allocator_t a, xqc_switch_pq_compare_ptr cmp)
{
    size_t element_size = sizeof(xqc_switch_pq_elem_t);
    if (capacity == 0)
    {
        return -1;
    }

    pq->elements = (char *)a.malloc(a.opaque, element_size * capacity);
    if (pq->elements == NULL)
    {
        return -2;
    }

    pq->element_size = element_size;
    pq->count = 0;
    pq->capacity = capacity;
    pq->a = a;
    pq->cmp = cmp;
    pq->build_time = xqc_monotonic_timestamp();

    return 0;
}

static inline int
xqc_switch_pq_init_default(xqc_switch_pq_t *pq)
{
    return xqc_switch_pq_init(pq, xqc_switch_pq_default_capacity, xqc_default_allocator, xqc_switch_pq_default_cmp);
}

static inline void
xqc_switch_pq_destroy(xqc_switch_pq_t *pq)
{
    pq->a.free(pq->a.opaque, pq->elements);
    pq->elements = NULL;
    pq->element_size = 0;
    pq->count = 0;
    pq->capacity = 0;
}

static inline void
xqc_switch_pq_element_swap(xqc_switch_pq_t *pq, size_t i, size_t j)
{
    char buf[pq->element_size];

    memcpy(buf, xqc_switch_pq_element(pq, j), pq->element_size);
    memcpy(xqc_switch_pq_element(pq, j), xqc_switch_pq_element(pq, i), pq->element_size);
    memcpy(xqc_switch_pq_element(pq, i), buf, pq->element_size);
}

static inline xqc_switch_pq_elem_t *
xqc_switch_pq_push(xqc_switch_pq_t *pq, float metric, xqc_switch_CCA_t switch_CCA, xqc_usec_t now)
{
    if (pq->count == pq->capacity)
    {
        size_t capacity = pq->capacity * 2;
        size_t size = capacity * pq->element_size;
        void *buf = pq->a.malloc(pq->a.opaque, size);
        if (buf == NULL)
        {
            return NULL;
        }
        memcpy(buf, pq->elements, pq->capacity * pq->element_size);
        pq->a.free(pq->a.opaque, pq->elements);
        pq->elements = (char *)buf;
        pq->capacity = capacity;
    }

    xqc_switch_pq_elem_t *p = xqc_switch_pq_element(pq, pq->count);
    p->metric = metric;
    p->switch_CCA = switch_CCA;
    p->last_use_time = now;

    size_t i = pq->count++;
    while (i != 0)
    {
        int j = (i - 1) / 2;
        if (!pq->cmp(xqc_switch_pq_element(pq, j)->metric, xqc_switch_pq_element(pq, i)->metric))
            break;

        xqc_switch_pq_element_swap(pq, i, j);

        i = j;
    }

    return xqc_switch_pq_element(pq, i);
}

static inline xqc_switch_pq_elem_t *
xqc_switch_pq_top(xqc_switch_pq_t *pq)
{
    if (pq->count == 0)
    {
        return NULL;
    }
    return xqc_switch_pq_element(pq, 0);
}

static inline int
xqc_switch_pq_empty(xqc_switch_pq_t *pq)
{
    return pq->count == 0 ? 1 : 0;
}

static inline void
xqc_switch_pq_pop(xqc_switch_pq_t *pq)
{
    if (pq->count == 0 || --pq->count == 0)
    {
        return;
    }

    xqc_switch_pq_element_copy(pq, 0, pq->count);
    xqc_switch_pq_elem_t *p = xqc_switch_pq_element(pq, 0);

    int i = 0, j = 2 * i + 1;
    while (j <= pq->count - 1)
    {
        if (j < pq->count - 1 && pq->cmp(xqc_switch_pq_element(pq, j)->metric, xqc_switch_pq_element(pq, j + 1)->metric))
        {
            ++j;
        }

        if (!pq->cmp(xqc_switch_pq_element(pq, i)->metric, xqc_switch_pq_element(pq, j)->metric))
        {
            break;
        }

        xqc_switch_pq_element_swap(pq, i, j);

        i = j;
        j = 2 * i + 1;
    }
}

static inline void
xqc_switch_pq_add_metric_each(xqc_switch_pq_t *pq, float add, float max)
{
    for (int i = 0; i < pq->count; i++)
    {
        xqc_switch_pq_elem_t *elem = xqc_switch_pq_element(pq, i);
        elem->metric = xqc_min(max, elem->metric + add);
    }
}

static inline void
xqc_switch_pq_rebuild(xqc_switch_pq_t *pq) {
    float metric_buf[XQC_CCA_NUM];
    xqc_usec_t time_buf[XQC_CCA_NUM];
    xqc_memzero(metric_buf, sizeof(float) * XQC_CCA_NUM);
    xqc_memzero(time_buf, sizeof(xqc_usec_t) * XQC_CCA_NUM);
    while (!xqc_switch_pq_empty(pq))
    {
        xqc_switch_pq_elem_t *elem = xqc_switch_pq_top(pq);
        xqc_switch_CCA_t state = elem->switch_CCA;
        float metric = elem->metric;
        metric_buf[state] = metric;
        time_buf[state] = elem->last_use_time;
        xqc_switch_pq_pop(pq);
    }
    for (int i = 0; i < XQC_CCA_NUM; i++)
    {
        if (metric_buf[i] != 0) {
            xqc_switch_pq_push(pq, metric_buf[i], i, time_buf[i]);
        }
    }
}

static inline void
xqc_switch_pq_print(xqc_switch_pq_t *pq, xqc_connection_t *conn)
{
    const char *cong_ctrl_str[] = {

        "cubic",
#ifndef XQC_DISABLE_RENO
        "reno",
#endif
        "copa",
        "bbr",
#ifdef XQC_ENABLE_BBR2
        "bbr2",
#endif
        "coward"
        // "unlimited"
    };
    for (int i = 0; i < pq->count; i++)
    {
        xqc_switch_pq_elem_t *elem = xqc_switch_pq_element(pq, i);
        xqc_extra_log(conn->log, conn->CS_extra_log, "[CCA:%s] [metric:%.6f]", cong_ctrl_str[elem->switch_CCA], elem->metric);
    }
}

// static inline void
// xqc_switch_pq_remove(xqc_switch_pq_t *pq, struct xqc_connection_s *conn)
// {
//     unsigned pq_index = conn->switch_pq_index;
//     if (pq_index >= pq->count || pq->count == 0 || --pq->count == 0) {
//         return;
//     }

//     xqc_switch_pq_element_copy(pq, pq_index, pq->count);
//     xqc_switch_pq_elem_t* p = xqc_switch_pq_element(pq, pq_index);
//     p->conn->switch_pq_index = pq_index;

//     int i = pq_index, j = 2 * i + 1;
//     while (j <= pq->count - 1) {
//         if (j < pq->count - 1 && pq->cmp(xqc_switch_pq_element(pq, j)->switch_time, xqc_switch_pq_element(pq, j+1)->switch_time)) {
//             ++j;
//         }

//         if (!pq->cmp(xqc_switch_pq_element(pq, i)->switch_time, xqc_switch_pq_element(pq, j)->switch_time)) {
//             break;
//         }

//         xqc_switch_pq_element_swap(pq, i, j);

//         i = j;
//         j = 2 * i + 1;
//     }

//     i = pq_index;
//     while (i != 0) {
//         j = (i - 1)/2;
//         if (!pq->cmp(xqc_switch_pq_element(pq, j)->switch_time, xqc_switch_pq_element(pq, i)->switch_time)) {
//             break;
//         }

//         xqc_switch_pq_element_swap(pq, i, j);
//         i = j;
//     }
// }

// #undef xqc_switch_pq_element
#undef xqc_switch_pq_element_copy

#endif