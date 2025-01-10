#include "src/common/xqc_hash.h"
#include "src/common/xqc_str_hash.h"
#include "src/common/xqc_malloc.h"
#include "src/transport/xqc_CCA_info_container.h"

int xqc_CCA_info_container_init(xqc_CCA_info_container_t *container, xqc_CCA_info_container_type_t type)
{
  if (type < XQC_CCA_INFO_HASH || type >= XQC_CCA_INFO_TYPE_NUM)
  {
    return -1;
  }
  container->type = type;
  switch (type)
  {
  case XQC_CCA_INFO_HASH:
    container->raw = xqc_malloc(sizeof(xqc_str_hash_table_t));
    if (xqc_str_hash_init((xqc_str_hash_table_t *)container->raw, xqc_default_allocator, 10))
    {
      xqc_free(container->raw);
      return -XQC_EMALLOC;
    }
    break;
  case XQC_CCA_INFO_TRIE:
  default:
    break;
  }
  return XQC_OK;
}

void xqc_CCA_info_container_release(xqc_CCA_info_container_t *container)
{
  switch (container->type)
  {
  case XQC_CCA_INFO_HASH:
    xqc_str_hash_release((xqc_str_hash_table_t *)container->raw);
    break;
  case XQC_CCA_INFO_TRIE:
  default:
    break;
  }
}

void *
xqc_CCA_info_container_find(xqc_CCA_info_container_t *container, char *addr, size_t len)
{
  void *res = NULL;
  switch (container->type)
  {
  case XQC_CCA_INFO_HASH:
  {
    uint64_t hash = xqc_hash_string(addr, len);
    xqc_str_t str = {
        .data = (unsigned char *)addr,
        .len = len,
    };
    res = xqc_str_hash_find((xqc_str_hash_table_t *)container->raw, hash, str);
  }
  break;
  case XQC_CCA_INFO_TRIE:
  default:
    break;
  }
  return res;
}

int xqc_CCA_info_container_add(xqc_CCA_info_container_t *container, char *addr, size_t len, xqc_ip_CCA_info_t **CCA_info)
{
  switch (container->type)
  {
  case XQC_CCA_INFO_HASH:
  {
    uint64_t hash = xqc_hash_string(addr, len);

    xqc_str_hash_element_t c = {
        .str = {
            .data = (unsigned char *)addr,
            .len = len},
        .hash = hash,
        .value = CCA_info};

    if (xqc_str_hash_add((xqc_str_hash_table_t *)container->raw, c))
    {
      return -XQC_EMALLOC;
    }
  }
  break;
  case XQC_CCA_INFO_TRIE:
  default:
    break;
  }
  return XQC_OK;
}

int xqc_CCA_info_container_delete(xqc_CCA_info_container_t *container, char *addr, size_t len)
{
  switch (container->type)
  {
  case XQC_CCA_INFO_HASH:
  {
    uint64_t hash = xqc_hash_string(addr, len);
    xqc_str_t str = {
        .data = (unsigned char *)addr,
        .len = len,
    };

    if (xqc_str_hash_delete((xqc_str_hash_table_t *)container->raw, hash, str))
    {
      return -XQC_ECONN_NFOUND;
    }
  }
  break;
  case XQC_CCA_INFO_TRIE:
  default:
    break;
  }
  return XQC_OK;
}

xqc_ip_CCA_info_t **
xqc_CCA_info_container_get(xqc_CCA_info_container_t *container, char *addr, size_t *len)
{
  xqc_ip_CCA_info_t **res = NULL;
  switch (container->type)
  {
  case XQC_CCA_INFO_HASH:
    res = (xqc_ip_CCA_info_t **)xqc_str_hash_get((xqc_str_hash_table_t *)container->raw, addr, len);
    break;
  case XQC_CCA_INFO_TRIE:
  default:
    break;
  }
  return res;
}

void
xqc_CCA_info_container_free_value(xqc_ip_CCA_info_t **mp_map)
{
  for (size_t i = 0; i < XQC_MAX_PATHS_COUNT; i++)
  {
    xqc_ip_CCA_info_t *CCA_info = mp_map[i];
    if (CCA_info) {
      xqc_free(CCA_info);
    }
  }
  xqc_free(mp_map);
}