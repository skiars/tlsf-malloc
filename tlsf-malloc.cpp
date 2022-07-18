#include "tlsf-malloc.h"
#include "tlsf-malloc.hpp"

typedef tlsf::heap<> tlsf_heap_base;
struct tlsf_heap : public tlsf::heap<> {};

tlsf_t tlsf_create_with_pool(void *mem, size_t size) {
    return (tlsf_heap *) tlsf_heap_base::create_with_pool(mem, size);
}

void *tlsf_malloc(tlsf_t heap, size_t size) {
    return heap->malloc(size);
}

void tlsf_free(tlsf_t heap, void *ptr) {
    heap->free(ptr);
}

void *tlsf_realloc(tlsf_t heap, void *ptr, size_t size) {
    return heap->realloc(ptr, size);
}
