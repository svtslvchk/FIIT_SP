#include <not_implemented.h>
#include "../include/allocator_sorted_list.h"

struct allocator_metadata {
    std::pmr::memory_resource *parent_allocator;
    allocator_with_fit_mode::fit_mode fit_mode;
    size_t total_size;
    std::mutex sync;
    void *first_free_block;
};

struct block_metadata {
    size_t size;
    void *next_free;
};

static inline size_t align_size(size_t size) {
    constexpr size_t alignment = alignof(std::max_align_t);
    return (size + alignment - 1) & ~(alignment - 1);
}

allocator_sorted_list::~allocator_sorted_list()
{
    auto *meta = reinterpret_cast<allocator_metadata *>(_trusted_memory);
    meta->sync.~mutex();
    size_t total_size = meta->total_size;
    std::pmr::memory_resource *parent = meta->parent_allocator;

    if (parent) {
        parent->deallocate(_trusted_memory, total_size);
    } else {
        ::operator delete(_trusted_memory);
    }
}

allocator_sorted_list::allocator_sorted_list(
    allocator_sorted_list &&other) noexcept: _trusted_memory(other._trusted_memory)
{
    other._trusted_memory = nullptr;
}

allocator_sorted_list &allocator_sorted_list::operator=(
    allocator_sorted_list &&other) noexcept
{
    if (this != &other) {
        _trusted_memory = other._trusted_memory;
        other._trusted_memory = nullptr;
    }

    return *this;
}

allocator_sorted_list::allocator_sorted_list(
        size_t space_size,
        std::pmr::memory_resource *parent_allocator,
        allocator_with_fit_mode::fit_mode allocate_fit_mode)
{
    size_t metadata_sz = align_size(sizeof(allocator_metadata));
    size_t block_ndr_sz = align_size(sizeof(block_metadata));

    if (space_size < block_ndr_sz) {
        throw std::logic_error("Requested size is too small for allocator overhead");
    }

    size_t total_requested = metadata_sz + space_size;

    if (parent_allocator) {
        _trusted_memory = parent_allocator->allocate(total_requested);
    } else {
        _trusted_memory = ::operator new(total_requested);
    }

    auto *meta = reinterpret_cast<allocator_metadata *>(_trusted_memory);
    meta->parent_allocator = parent_allocator;
    meta->fit_mode = allocate_fit_mode;
    meta->total_size = total_requested;
    new (&meta->sync) std::mutex();

    void *first_block_ptr = reinterpret_cast<std::byte *>(_trusted_memory) + metadata_sz;
    auto *first_block = reinterpret_cast<block_metadata *>(first_block_ptr);
    first_block->size = space_size;
    first_block->next_free = nullptr;

    meta->first_free_block = first_block_ptr;
}

[[nodiscard]] void *allocator_sorted_list::do_allocate_sm(
    size_t size)
{
    auto *meta = reinterpret_cast<allocator_metadata *>(_trusted_memory);
    std::lock_guard<std::mutex> lock(meta->sync);

    size_t requested_total = align_size(size + sizeof(block_metadata));
    size_t block_ndr_sz = align_size(sizeof(block_metadata));
    block_metadata *prev = nullptr;
    block_metadata *cur = reinterpret_cast<block_metadata *>(meta->first_free_block);
    block_metadata *target = nullptr;
    block_metadata *target_prev = nullptr;

    while (cur) {
        if (cur->size >= requested_total) {
            if (meta->fit_mode == fit_mode::first_fit) {
                target = cur;
                target_prev = prev;
                break;
            } else if (meta->fit_mode == fit_mode::the_best_fit) {
                if (!target || cur->size < target->size) {
                    target = cur;
                    target_prev = prev;
                }
            } else if (meta->fit_mode == fit_mode::the_worst_fit) {
                if (!target || cur->size > target->size) {
                    target = cur;
                    target_prev = prev;
                }
            }
        }

        prev = cur;
        cur = reinterpret_cast<block_metadata *>(cur->next_free);
    }

    if (!target) {
        throw std::bad_alloc();
    }

    if (target->size >= requested_total + block_ndr_sz) {
        size_t remaining_size = target->size - requested_total;
        auto *new_free_block = reinterpret_cast<block_metadata *>(reinterpret_cast<std::byte *>(target) + requested_total);
        new_free_block->size = remaining_size;
        new_free_block->next_free = target->next_free;

        if (target_prev) {
            target_prev->next_free = new_free_block;
        } else {
            meta->first_free_block = new_free_block;
        }

        target->size = requested_total;
    } else {
        if (target_prev) {
            target_prev->next_free = target->next_free;
        } else {
            meta->first_free_block = target->next_free;
        }
    }

    target->next_free = nullptr;
    return reinterpret_cast<std::byte *>(target) + block_ndr_sz;
}

allocator_sorted_list::allocator_sorted_list(const allocator_sorted_list &other)
{
    throw std::logic_error("Copying of allocator_sorted_list is not supported");
}

allocator_sorted_list &allocator_sorted_list::operator=(const allocator_sorted_list &other)
{
    throw std::logic_error("Copying of allocator_sorted_list is not supported");
}

bool allocator_sorted_list::do_is_equal(const std::pmr::memory_resource &other) const noexcept
{
    return this == &other;
}

void allocator_sorted_list::do_deallocate_sm(
    void *at)
{
    if (!at) {
        return;
    }

    auto *meta = reinterpret_cast<allocator_metadata *>(_trusted_memory);
    std::lock_guard<std::mutex> lock(meta->sync);

    std::byte *at_ptr = static_cast<std::byte *>(at);
    std::byte *pool_start = reinterpret_cast<std::byte *>(_trusted_memory) + align_size(sizeof(allocator_metadata));
    std::byte *pool_end = reinterpret_cast<std::byte *>(_trusted_memory) + meta->total_size;

    if (at_ptr < pool_start || at_ptr >= pool_end) {
        throw std::logic_error("Pointer not managed by this allocator");
    }

    std::byte *curr_scan = pool_start;
    bool valid_pointer = false;
    size_t block_ndr_sz = align_size(sizeof(block_metadata));
    while (curr_scan < pool_end) {
        auto *block = reinterpret_cast<block_metadata *>(curr_scan);
        void *valid_data_start = curr_scan + block_ndr_sz; 

        if (valid_data_start == at) {
            valid_pointer = true;
            break;
        }
        if (valid_data_start > at) {
            break;
        }

        curr_scan += block->size;
    }

    if (!valid_pointer) {
        throw std::logic_error("Invalid pointer deallocation: not a start of a block");
    }
    
    auto *block_to_free = reinterpret_cast<block_metadata *>(
        reinterpret_cast<std::byte *>(at) - align_size(sizeof(block_metadata))
    );

    block_metadata *prev = nullptr;
    block_metadata *cur = reinterpret_cast<block_metadata *>(meta->first_free_block);

    while (cur && cur < block_to_free) {
        prev = cur;
        cur = reinterpret_cast<block_metadata *>(cur->next_free);
    }

    block_to_free->next_free = cur;
    if (prev) {
        prev->next_free = block_to_free;
    } else {
        meta->first_free_block = block_to_free;
    }

    // склейка с правым
    if (cur && reinterpret_cast<std::byte *>(block_to_free) + block_to_free->size == reinterpret_cast<std::byte *>(cur)) {
        block_to_free->size += cur->size;
        block_to_free->next_free = cur->next_free;
    }

    // склейка с левым
    if (prev && reinterpret_cast<std::byte *>(prev) + prev->size == reinterpret_cast<std::byte *>(block_to_free)) {
        prev->size += block_to_free->size;
        prev->next_free = block_to_free->next_free;
    }
}

inline void allocator_sorted_list::set_fit_mode(
    allocator_with_fit_mode::fit_mode mode)
{
    auto *meta = reinterpret_cast<allocator_metadata *>(_trusted_memory);
    std::lock_guard<std::mutex> lock(meta->sync);
    meta->fit_mode = mode;
}

std::vector<allocator_test_utils::block_info> allocator_sorted_list::get_blocks_info() const noexcept
{
    auto *meta = reinterpret_cast<allocator_metadata *>(_trusted_memory);
    std::lock_guard<std::mutex> lock(meta->sync);
    return get_blocks_info_inner();
}


std::vector<allocator_test_utils::block_info> allocator_sorted_list::get_blocks_info_inner() const
{
    std::vector<allocator_test_utils::block_info> info;
    auto *meta = reinterpret_cast<allocator_metadata *>(_trusted_memory);
    size_t metadata_sz = align_size(sizeof(allocator_metadata));
    std::byte *cur_ptr = reinterpret_cast<std::byte *>(_trusted_memory) + metadata_sz;
    std::byte *end_ptr = reinterpret_cast<std::byte *>(_trusted_memory) + meta->total_size;

    while (cur_ptr < end_ptr) {
        auto *block = reinterpret_cast<block_metadata *>(cur_ptr);
        bool is_free = false;
        block_metadata *free_cur = reinterpret_cast<block_metadata *>(meta->first_free_block);

        while (free_cur) {
            if (free_cur == block) {
                is_free = true;
                break;
            }

            free_cur = reinterpret_cast<block_metadata *>(free_cur->next_free);
        }

        info.push_back({block->size, !is_free});
        cur_ptr += block->size;
    }

    return info;
}

namespace {
    inline void* get_first_free_block(void* trusted) noexcept {
        std::byte* ptr = static_cast<std::byte*>(trusted);
        ptr += sizeof(std::pmr::memory_resource*);
        ptr += sizeof(allocator_with_fit_mode::fit_mode);
        ptr += sizeof(size_t);
        ptr += sizeof(std::mutex);
        return *reinterpret_cast<void**>(ptr);
    }

    inline size_t get_trusted_total_size(void* trusted) noexcept {
        std::byte* ptr = static_cast<std::byte*>(trusted);
        ptr += sizeof(std::pmr::memory_resource*);
        ptr += sizeof(allocator_with_fit_mode::fit_mode);
        return *reinterpret_cast<size_t*>(ptr);
    }

    inline void* get_next_free_ptr(void* block) noexcept {
        return *reinterpret_cast<void**>(block);
    }

    inline size_t get_block_size(void* block) noexcept {
        std::byte* ptr = static_cast<std::byte*>(block);
        ptr += sizeof(void*);
        return *reinterpret_cast<size_t*>(ptr);
    }
}

allocator_sorted_list::sorted_free_iterator allocator_sorted_list::free_begin() const noexcept
{
    if (!_trusted_memory){
        return sorted_free_iterator(nullptr);
    }

    return sorted_free_iterator(get_first_free_block(_trusted_memory));
}

allocator_sorted_list::sorted_free_iterator allocator_sorted_list::free_end() const noexcept
{
    return sorted_free_iterator(nullptr);
}

allocator_sorted_list::sorted_iterator allocator_sorted_list::begin() const noexcept
{
    return sorted_iterator(_trusted_memory);
}

allocator_sorted_list::sorted_iterator allocator_sorted_list::end() const noexcept
{
    return sorted_iterator(nullptr);
}

bool allocator_sorted_list::sorted_free_iterator::operator==(
        const allocator_sorted_list::sorted_free_iterator & other) const noexcept
{
    return _free_ptr == other._free_ptr;
}

bool allocator_sorted_list::sorted_free_iterator::operator!=(
        const allocator_sorted_list::sorted_free_iterator &other) const noexcept
{
    return _free_ptr != other._free_ptr;
}

allocator_sorted_list::sorted_free_iterator &allocator_sorted_list::sorted_free_iterator::operator++() & noexcept
{
    if (_free_ptr) {
        _free_ptr = get_next_free_ptr(_free_ptr);
    }

    return *this;
}

allocator_sorted_list::sorted_free_iterator allocator_sorted_list::sorted_free_iterator::operator++(int n)
{
    sorted_free_iterator temp = *this;
    ++(*this);
    return temp;
}

size_t allocator_sorted_list::sorted_free_iterator::size() const noexcept
{
    return get_block_size(_free_ptr);
}

void *allocator_sorted_list::sorted_free_iterator::operator*() const noexcept
{
    return static_cast<std::byte *>(_free_ptr) + allocator_sorted_list::block_metadata_size;
}

allocator_sorted_list::sorted_free_iterator::sorted_free_iterator(): _free_ptr(nullptr)
{
}

allocator_sorted_list::sorted_free_iterator::sorted_free_iterator(void *trusted): _free_ptr(trusted)
{
}

bool allocator_sorted_list::sorted_iterator::operator==(const allocator_sorted_list::sorted_iterator & other) const noexcept
{
    return _current_ptr == other._current_ptr;
}

bool allocator_sorted_list::sorted_iterator::operator!=(const allocator_sorted_list::sorted_iterator &other) const noexcept
{
    return _current_ptr != other._current_ptr;
}

allocator_sorted_list::sorted_iterator &allocator_sorted_list::sorted_iterator::operator++() & noexcept
{
    if (!_current_ptr) {
        return *this;
    }

    size_t current_block_size = get_block_size(_current_ptr);
    _current_ptr = static_cast<std::byte*>(_current_ptr) + current_block_size;
    size_t total_size = get_trusted_total_size(_trusted_memory);
    std::byte *memory_end = static_cast<std::byte *>(_trusted_memory) + total_size;

    if (static_cast<std::byte *>(_current_ptr) >= memory_end) {
        _current_ptr = nullptr;
        _free_ptr = nullptr;
    } else {
        while (_free_ptr && static_cast<std::byte *>(_free_ptr) < static_cast<std::byte *>(_current_ptr)) {
            _free_ptr = get_next_free_ptr(_free_ptr);
        }
    }
    
    return *this;
}

allocator_sorted_list::sorted_iterator allocator_sorted_list::sorted_iterator::operator++(int n)
{
    sorted_iterator temp = *this;
    ++(*this);
    return temp;
}

size_t allocator_sorted_list::sorted_iterator::size() const noexcept
{
    return get_block_size(_current_ptr);
}

void *allocator_sorted_list::sorted_iterator::operator*() const noexcept
{
    return static_cast<std::byte *>(_current_ptr) + allocator_sorted_list::block_metadata_size;
}

allocator_sorted_list::sorted_iterator::sorted_iterator(): _free_ptr(nullptr), _current_ptr(nullptr), _trusted_memory(nullptr)
{
}

allocator_sorted_list::sorted_iterator::sorted_iterator(void *trusted): _trusted_memory(trusted)
{
    if (!trusted) {
        _free_ptr = nullptr;
        _current_ptr = nullptr;
    } else {
        _current_ptr = static_cast<std::byte *>(trusted) + allocator_sorted_list::allocator_metadata_size;
        _free_ptr = get_first_free_block(trusted);
        size_t total_size = get_trusted_total_size(trusted);
        if (allocator_sorted_list::allocator_metadata_size >= total_size) {
            _current_ptr = nullptr;
        }
    }
}

bool allocator_sorted_list::sorted_iterator::occupied() const noexcept
{
    return _current_ptr != _free_ptr;
}
