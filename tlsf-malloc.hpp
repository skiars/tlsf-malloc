#ifndef __TLSF_MALLOC_H__
#define __TLSF_MALLOC_H__

#include <cstddef>
#include <cstring>

#if !defined(TLSF_NDEBUG)
#include <cassert>
#define tlsf_assert         assert
#else
#define tlsf_assert(e)      ((void)0)
#endif

#ifdef _MSC_VER
#include <intrin.h>
#pragma intrinsic(_BitScanForward)
#pragma intrinsic(_BitScanReverse)
#endif

#if __cplusplus > 199711l
#define tlsf_static_assert  static_assert
#else
#define tlsf_static_assert(...)
#endif

namespace tlsf {
template<int align_size_log2 = 3, int fl_index_max = 30>
class heap {
public:
    tlsf_static_assert(align_size_log2 < 5, "invalid parameter");
    tlsf_static_assert(fl_index_max >= 10 && fl_index_max < 32, "invalid parameter");
    tlsf_static_assert(fl_index_max - align_size_log2 > 5, "invalid parameter");

    static const std::size_t align_size = 1 << align_size_log2;

private:
    enum {
        SL_INDEX_COUNT_LOG2 = 5,
        SL_INDEX_COUNT = (1 << SL_INDEX_COUNT_LOG2),
        FL_INDEX_SHIFT = (SL_INDEX_COUNT_LOG2 + align_size_log2),
        FL_INDEX_COUNT = (fl_index_max - FL_INDEX_SHIFT + 1),
        SMALL_BLOCK_SIZE = (1 << FL_INDEX_SHIFT),
    };

    template<std::size_t size> struct static_align {
        static const std::size_t value = (size + (align_size - 1)) & ~(align_size - 1);
    };

    struct block_header {
        block_header *prev_phys;
        std::size_t size_masks;
        block_header *prev_free;
        block_header *next_free;

        std::size_t size() const { return size_masks & ~3; }
        bool is_last() const { return size() == 0; }
        bool is_free() const { return size_masks & 1; }
        bool is_prev_free() const { return size_masks & 2; }
        void set_size(std::size_t size) { size_masks = size | (size_masks & 3); }
        void set_used_mask() { size_masks &= ~1; }
        void set_free_mask() { size_masks |= 1; }
        void set_prev_used_mask() { size_masks &= ~2; }
        void set_prev_free_mask() { size_masks |= 2; }

        bool can_split(std::size_t size) const {
            return this->size() >= size + block_header_overhead + min_block_size;
        }
        block_header *next_phys() const {
            tlsf_assert(!is_last());
            return offset_to_block(this, std::ptrdiff_t(block_header_overhead + size()));
        }
        block_header *link_next() {
            block_header *next = next_phys();
            next->prev_phys = this;
            return next;
        }
        void mark_used() {
            block_header *next = next_phys();
            next->set_prev_used_mask();
            set_used_mask();
        }
        void mark_free() {
            block_header *next = link_next();
            next->set_prev_free_mask();
            set_free_mask();
        }

        block_header *split(std::size_t size) {
            block_header *remaining = offset_to_block(this, std::ptrdiff_t(block_header_overhead + size));
            std::size_t remain_size = this->size() - size - block_header_overhead;
            tlsf_assert(remain_size >= min_block_size);
            tlsf_assert(block_to_ptr(remaining) == align_ptr(block_to_ptr(remaining)));
            set_size(size);
            link_next();
            remaining->set_size(remain_size);
            remaining->mark_free();
            return remaining;
        }
        block_header *merge(block_header *block) {
            tlsf_assert(!is_last());
            size_masks += block_header_overhead + block->size();
            link_next();
            return this;
        }
    };

    block_header block_null;
    unsigned int fl_bitmap;
    unsigned int sl_bitmap[FL_INDEX_COUNT];
    block_header *blocks[FL_INDEX_COUNT][SL_INDEX_COUNT];

    static const std::size_t block_header_overhead = static_align<sizeof(void *) + sizeof(std::size_t)>::value;
    static const std::size_t min_block_size = static_align<sizeof(block_header) - block_header_overhead>::value;
    static const std::size_t max_block_size = std::size_t(1) << fl_index_max;

    static std::size_t align_down(std::size_t x) { return x & ~(align_size - 1); }
    static std::size_t align_up(std::size_t x) { return (x + (align_size - 1)) & ~(align_size - 1); }
    template<class T> static T *align_ptr(T *ptr) { return reinterpret_cast<T *>(align_up(std::size_t(ptr))); }

    void init_heap() {
        block_null.prev_phys = 0;
        block_null.size_masks = 0;
        block_null.next_free = &block_null;
        block_null.prev_free = &block_null;

        fl_bitmap = 0;
        for (int i = 0; i < FL_INDEX_COUNT; ++i) {
            sl_bitmap[i] = 0;
            for (int j = 0; j < SL_INDEX_COUNT; ++j)
                blocks[i][j] = &block_null;
        }
    }

    static block_header *offset_to_block(const void *ptr, std::ptrdiff_t offset) {
        return reinterpret_cast<block_header *>(std::ptrdiff_t(ptr) + offset);
    }
    static void *block_to_ptr(const block_header *block) {
        return reinterpret_cast<void *>(std::ptrdiff_t(block) + block_header_overhead);
    }
    static block_header *block_from_ptr(const void *ptr) {
        return reinterpret_cast<block_header *>(std::ptrdiff_t(ptr) - block_header_overhead);
    }
    static std::size_t adjust_size(std::size_t size) {
        std::size_t aligned = align_up(size);
        return aligned < min_block_size ? min_block_size : aligned <= max_block_size ? aligned : 0;
    }

#if defined(_MSC_VER)
    static int ffs(unsigned x) {
        unsigned long i;
        return _BitScanForward(&i, x) ? i : 0;
    }
    static int fls(unsigned x) {
	    unsigned long i;
	    return _BitScanReverse(&i, x) ? i : -1;
    }
#elif defined(__GNUC__)
    static int ffs(unsigned x) { return __builtin_ffs(int(x)) - 1; }
    static int fls(unsigned x) {
        const int bit = x ? 32 - __builtin_clz(x) : 0;
        return bit - 1;
    }
#else
    #error "unsupport compiler"
#endif

    static void mapping_search(std::size_t size, int &fl, int &sl) {
        if (size >= SMALL_BLOCK_SIZE)
            size += (1 << (fls(unsigned(size)) - SL_INDEX_COUNT_LOG2)) - 1;
        mapping_insert(size, fl, sl);
    }

    static void mapping_insert(std::size_t size, int &fl, int &sl) {
        if (size < SMALL_BLOCK_SIZE) {
            fl = 0;
            sl = int(size / (SMALL_BLOCK_SIZE / SL_INDEX_COUNT));
        } else {
            fl = fls(unsigned(size));
            sl = int(size >> (fl - SL_INDEX_COUNT_LOG2)) ^ (1 << SL_INDEX_COUNT_LOG2);
            fl -= FL_INDEX_SHIFT - 1;
        }
    }

    block_header *search_suitable_block(int &fl, int &sl) {
        unsigned int sl_map = sl_bitmap[fl] & (~0u << sl);
        if (!sl_map) {
            /* No block exists. Search in the next largest first-level list. */
            const unsigned int fl_map = fl_bitmap & (~0u << (fl + 1));
            if (!fl_map) /* No free blocks available, memory has been exhausted. */
                return 0;
            fl = ffs(fl_map);
            sl_map = sl_bitmap[fl];
        }
        tlsf_assert(sl_map); // second level bitmap is null
        sl = ffs(sl_map);

        /* Return the first block in the free list. */
        return blocks[fl][sl];
    }

    void insert_free_block(block_header *block, int fl, int sl) {
        block_header *current = blocks[fl][sl];
        tlsf_assert(block); // cannot insert a null entry into the free list
        tlsf_assert(current); // free list cannot have a null entry
        block->next_free = current;
        block->prev_free = &block_null;
        current->prev_free = block;

        tlsf_assert(block_to_ptr(block) == align_ptr(block_to_ptr(block)));

        blocks[fl][sl] = block;
        fl_bitmap |= (1u << fl);
        sl_bitmap[fl] |= (1u << sl);
    }

    void remove_free_block(block_header *block, int fl, int sl) {
        block_header *prev = block->prev_free;
        block_header *next = block->next_free;
        tlsf_assert(prev && next);
        next->prev_free = prev;
        prev->next_free = next;
        if (blocks[fl][sl] == block) { /* If the new head is null, clear the bitmap. */
            blocks[fl][sl] = next;
            if (next == &block_null) {
                sl_bitmap[fl] &= ~(1u << sl);
                if (!sl_bitmap[fl]) /* If the second bitmap is now empty, clear the fl bitmap. */
                    fl_bitmap &= ~(1u << fl);
            }
        }
    }

    void insert_block(block_header *block) {
        int fl, sl;
        mapping_insert(block->size(), fl, sl);
        insert_free_block(block, fl, sl);
    }
    void block_remove(block_header *block) {
        int fl, sl;
        mapping_insert(block->size(), fl, sl);
        remove_free_block(block, fl, sl);
    }


    block_header *locate_free_block(std::size_t size) {
        int fl = 0, sl = 0;
        block_header *block = 0;
        if (size) {
            mapping_search(size, fl, sl);
            if (fl < FL_INDEX_COUNT)
                block = search_suitable_block(fl, sl);
        }
        if (block) {
            tlsf_assert(block->size() >= size);
            remove_free_block(block, fl, sl);
        }
        return block;
    }

    void *block_prepare_used(block_header *block, std::size_t size) {
        if (block) {
            tlsf_assert(size); // size must be non-zero
            block_trim_free(block, size);
            block->mark_used();
            return block_to_ptr(block);
        }
        return 0;
    }

    void block_trim_free(block_header *block, std::size_t size) {
        tlsf_assert(block->is_free());
        if (block->can_split(size)) {
            block_header *remaining_block = block->split(size);
            remaining_block->set_prev_free_mask();
            insert_block(remaining_block);
        }
    }

    void block_trim_used(block_header *block, std::size_t size) {
        tlsf_assert(!block->is_free()); /* block must be used */
        if (block->can_split(size)) {
            block_header *remaining_block = block->split(size);
            remaining_block->set_prev_used_mask();
            remaining_block = block_merge_next(remaining_block); /* If the next block is free, we must coalesce. */
            insert_block(remaining_block);
        }
    }

    block_header *block_merge_prev(block_header *block) {
        if (block->is_prev_free()) {
            block_header *prev = block->prev_phys;
            tlsf_assert(prev && prev->is_free());
            block_remove(prev);
            block = prev->merge(block);
        }
        return block;
    }

    block_header *block_merge_next(block_header *block) {
        block_header *next = block->next_phys();
        if (next->is_free()) {
            tlsf_assert(!block->is_last()); /* previous block can't be last */
            block_remove(next);
            block = block->merge(next);
        }
        return block;
    }

public:
    static heap *create_with_pool(void *mem, std::size_t size) {
        heap *h = reinterpret_cast<heap *>(mem);
        h->init_heap();
        h->add_pool(reinterpret_cast<char *>(mem) + sizeof(heap), size - sizeof(heap));
        return h;
    }

    void add_pool(void *mem, std::size_t size) {
        size = size > max_block_size ? max_block_size : align_down(size - block_header_overhead * 2);
        tlsf_assert((ptrdiff_t) mem % align_size == 0);
        tlsf_assert(size >= min_block_size);

        block_header *block = reinterpret_cast<block_header *>(mem);
        block->set_size(size);
        block->set_free_mask();
        block->set_prev_used_mask();
        insert_block(block);

        block_header *next = block->link_next();
        next->set_size(0);
        next->set_used_mask();
        next->set_prev_free_mask();
    }

    void *malloc(std::size_t size) {
        size = adjust_size(size);
        block_header *block = locate_free_block(size);
        return block_prepare_used(block, size);
    }

    void free(void *ptr) {
        if (ptr) {
            block_header *block = block_from_ptr(ptr);
            tlsf_assert(!block->is_free());
            block->mark_free();
            block = block_merge_prev(block);
            block = block_merge_next(block);
            insert_block(block);
        }
    }

    void *realloc(void *ptr, std::size_t size) {
        if (!ptr)
            return malloc(size);
        block_header *block = block_from_ptr(ptr);
        block_header *next = block->next_phys();
        std::size_t current = block->size();
        std::size_t combined = current + block_header_overhead + next->size();
        size = adjust_size(size);
        tlsf_assert(!block->is_free());
        if (size > current && (!next->is_free() || size > combined)) {
            void *p = malloc(size);
            if (p) {
                std::memcpy(p, ptr, current < size ? current : size);
                free(ptr);
            }
            return p;
        }
        if (size > current) {
            block_merge_next(block);
            block->mark_used();
        }
        block_trim_used(block, size);
        return ptr;
    }
};
}

#undef tlsf_static_assert
#undef tlsf_assert

#endif //! __TLSF_MALLOC_H__
