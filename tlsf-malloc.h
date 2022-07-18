#ifndef __TLSF_H__
#define __TLSF_H__

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tlsf_heap *tlsf_t;

tlsf_t tlsf_create_with_pool(void *mem, size_t size);
void *tlsf_malloc(tlsf_t heap, size_t size);
void tlsf_free(tlsf_t heap, void *ptr);
void *tlsf_realloc(tlsf_t heap, void *ptr, size_t size);

#ifdef __cplusplus
}
#endif

#endif //__TLSF_H__
