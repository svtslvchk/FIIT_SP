#include <not_implemented.h>
#include <new>
#include <stdexcept>
#include "../include/allocator_boundary_tags.h"

namespace {
    struct allocator_metadata {
        std::pmr::memory_resource *parent_allocator;
        allocator_with_fit_mode::fit_mode fit_mode;
        size_t total_size;
        std::mutex sync;
        void *first_block;
    };

    struct block_metadata {
        size_t size_and_flag;
        void *prev_block;
        void *next_block;
        void *trusted_memory; // указатель на начало аллокатора
    };

    inline bool is_occupied(size_t size_field) noexcept {
        return (size_field & 1) != 0;
    }

    inline size_t get_size(size_t size_field) noexcept {
        return size_field & ~static_cast<size_t>(1);
    }

    inline size_t pack_size_and_flag(size_t size, bool occupied) noexcept {
        return size | (occupied ? 1 : 0);
    }

    inline size_t align_size(size_t size) noexcept {
        constexpr size_t alignment = alignof(std::max_align_t);
        return (size + alignment - 1) & ~(alignment - 1);
    }
}

allocator_boundary_tags::~allocator_boundary_tags()
{
    if (_trusted_memory) {
        return;
    }

    auto *meta = reinterpret_cast<allocator_metadata *>(_trusted_memory);
    meta->sync.~mutex();
    size_t size = meta->total_size;
    std::pmr::memory_resource *parent = meta->parent_allocator;
    if (parent) {
        parent->deallocate(_trusted_memory, size);
    } else {
        ::operator delete(_trusted_memory);
    }
}

allocator_boundary_tags::allocator_boundary_tags(
    allocator_boundary_tags &&other) noexcept: _trusted_memory(other._trusted_memory)
{
    other._trusted_memory = nullptr;
}

allocator_boundary_tags &allocator_boundary_tags::operator=(
    allocator_boundary_tags &&other) noexcept
{
    if (this != &other) {
        this->~allocator_boundary_tags();
        _trusted_memory = other._trusted_memory;
        other._trusted_memory = nullptr;
    }

    return *this;
}


/** If parent_allocator* == nullptr you should use std::pmr::get_default_resource()
 */
allocator_boundary_tags::allocator_boundary_tags(
        size_t space_size,
        std::pmr::memory_resource *parent_allocator,
        allocator_with_fit_mode::fit_mode allocate_fit_mode)
{
    size_t meta_sz = align_size(sizeof(allocator_metadata));
    size_t block_meta_sz = align_size(sizeof(block_metadata));
    if (space_size < block_meta_sz) {
        throw std::logic_error("Requested size is too small for boundary tags overhead");
    }

    size_t total_requested = meta_sz + space_size;
    if (!parent_allocator) {
        parent_allocator = std::pmr::get_default_resource();
    }

    _trusted_memory = parent_allocator->allocate(total_requested);

    auto *meta = reinterpret_cast<allocator_metadata *>(_trusted_memory);
    meta->parent_allocator = parent_allocator;
    meta->fit_mode = allocate_fit_mode;
    meta->total_size = total_requested;
    new (&meta->sync) std::mutex();

    void *first_block_ptr = static_cast<std::byte *>(_trusted_memory) + meta_sz;
    auto *first_block = reinterpret_cast<block_metadata *>(first_block_ptr);

    first_block->size_and_flag = pack_size_and_flag(space_size, false);
    first_block->prev_block = nullptr;
    first_block->next_block = nullptr;
    first_block->trusted_memory = _trusted_memory;

    meta->first_block = first_block_ptr;
}

[[nodiscard]] void *allocator_boundary_tags::do_allocate_sm(
    size_t size)
{
    auto *meta = reinterpret_cast<allocator_metadata *>(_trusted_memory);
    std::lock_guard<std::mutex> lock(meta->sync);

    size_t block_meta_sz = align_size(sizeof(block_metadata));
    size_t requested_size = align_size(size);
    size_t total_size = block_meta_sz + requested_size;

    block_metadata *target_block = nullptr;
    block_metadata *current = reinterpret_cast<block_metadata *>(meta->first_block);

    size_t best_size = static_cast<size_t>(-1);
    size_t worst_size = 0;

    while (current != nullptr) {
        size_t cur_sz = get_size(current->size_and_flag);
        bool cur_busy = is_occupied(current->size_and_flag);

        if (!cur_busy && cur_sz >= total_size) {
            if (meta->fit_mode == allocator_with_fit_mode::fit_mode::first_fit) {
                // 1. First Fit: Нашли и сразу выходим
                target_block = current;
                break; 
            } else if (meta->fit_mode == allocator_with_fit_mode::fit_mode::the_best_fit) {
                // 2. Best Fit: Ищем блок, который минимально превышает нужный размер
                // (чтобы оставить меньше "хлама" в памяти)
                if (cur_sz < best_size) {
                    best_size = cur_sz;
                    target_block = current;
                }
            } else if (meta->fit_mode == allocator_with_fit_mode::fit_mode::the_worst_fit) {
                // 3. Worst Fit: Ищем самый огромный блок
                // (чтобы после "отрезания" остаток всё еще был полезным и большим)
                if (cur_sz > worst_size) {
                    worst_size = cur_sz;
                    target_block = current;
                }
            }
        }
        
        current = reinterpret_cast<block_metadata *>(current->next_block);
    }

    if (!target_block) {
        throw std::bad_alloc();
    }

    // Разрезание блока (Split)
    size_t target_sz = get_size(target_block->size_and_flag);
    if (target_sz >= total_size + block_meta_sz + 1) {
        void *new_block_ptr = reinterpret_cast<std::byte *>(target_block) + total_size;
        auto *new_block = reinterpret_cast<block_metadata *>(new_block_ptr);

        new_block->size_and_flag = pack_size_and_flag(target_sz - total_size, false);
        new_block->trusted_memory = _trusted_memory;        
        new_block->prev_block = target_block;
        new_block->next_block = target_block->next_block;

        if (target_block->next_block) {
            reinterpret_cast<block_metadata *>(target_block->next_block)->prev_block = new_block;
        }
        
        target_block->next_block = new_block_ptr;
        target_block->size_and_flag = pack_size_and_flag(total_size, true);
    } else {
        // Если блок не режем, просто помечаем целиком как занятый
        target_block->size_and_flag = pack_size_and_flag(target_sz, true);
    }

    return reinterpret_cast<std::byte *>(target_block) + block_meta_sz;
}

void allocator_boundary_tags::do_deallocate_sm(
    void *at)
{
    if (at == nullptr) {
        return;
    }

    auto *meta = reinterpret_cast<allocator_metadata *>(_trusted_memory);
    std::lock_guard<std::mutex> lock(meta->sync);
    size_t block_meta_sz = align_size(sizeof(block_metadata));
    auto *cur_block = reinterpret_cast<block_metadata *>(
        static_cast<std::byte *>(at) - block_meta_sz
    );

    if (cur_block->trusted_memory != _trusted_memory) {
        return;
    }

    // флаг
    size_t cur_sz = get_size(cur_block->size_and_flag);
    cur_block->size_and_flag = pack_size_and_flag(cur_sz, false);

    // склейка с правым
    if (cur_block->next_block) {
        auto *next = reinterpret_cast<block_metadata *>(cur_block->next_block);

        if (!is_occupied(next->size_and_flag)) {
            cur_sz += get_size(next->size_and_flag);
            cur_block->size_and_flag = pack_size_and_flag(cur_sz, false);
            cur_block->next_block = next->next_block;
            if (next->next_block) {
                reinterpret_cast<block_metadata *>(next->next_block)->prev_block = cur_block;
            }
        }
    }

    // склейка с левым
    if (cur_block->prev_block) {
        auto *prev = reinterpret_cast<block_metadata *>(cur_block->prev_block);
        if (!is_occupied(prev->size_and_flag)) {
            size_t prev_sz = get_size(prev->size_and_flag);
            prev_sz += cur_sz;
            prev->size_and_flag = pack_size_and_flag(prev_sz, false);
            prev->next_block = cur_block->next_block;
            if (cur_block->next_block) {
                reinterpret_cast<block_metadata *>(cur_block->next_block)->prev_block = prev;
            }

            cur_block = prev;
        }
    }
}

inline void allocator_boundary_tags::set_fit_mode(
    allocator_with_fit_mode::fit_mode mode)
{
    auto *meta = reinterpret_cast<allocator_metadata *>(_trusted_memory);
    std::lock_guard<std::mutex> lock(meta->sync);
    meta->fit_mode = mode;
}


std::vector<allocator_test_utils::block_info> allocator_boundary_tags::get_blocks_info() const
{
    throw not_implemented("std::vector<allocator_test_utils::block_info> allocator_boundary_tags::get_blocks_info() const", "your code should be here...");
}

allocator_boundary_tags::boundary_iterator allocator_boundary_tags::begin() const noexcept
{
    throw not_implemented("allocator_boundary_tags::boundary_iterator allocator_boundary_tags::begin() const noexcept", "your code should be here...");
}

allocator_boundary_tags::boundary_iterator allocator_boundary_tags::end() const noexcept
{
    throw not_implemented("allocator_boundary_tags::boundary_iterator allocator_boundary_tags::end() const noexcept", "your code should be here...");
}

std::vector<allocator_test_utils::block_info> allocator_boundary_tags::get_blocks_info_inner() const
{
    throw not_implemented("std::vector<allocator_test_utils::block_info> allocator_boundary_tags::get_blocks_info_inner() const", "your code should be here...");
}

allocator_boundary_tags::allocator_boundary_tags(const allocator_boundary_tags &other)
{
    throw std::logic_error("Copying is disabled for this allocator");
}

allocator_boundary_tags &allocator_boundary_tags::operator=(const allocator_boundary_tags &other)
{
    throw std::logic_error("Copying is disabled for this allocator");
}

bool allocator_boundary_tags::do_is_equal(const std::pmr::memory_resource &other) const noexcept
{
    return this == &other;
}

bool allocator_boundary_tags::boundary_iterator::operator==(
        const allocator_boundary_tags::boundary_iterator &other) const noexcept
{
    throw not_implemented("bool allocator_boundary_tags::boundary_iterator::operator==(const allocator_boundary_tags::boundary_iterator &) const noexcept", "your code should be here...");
}

bool allocator_boundary_tags::boundary_iterator::operator!=(
        const allocator_boundary_tags::boundary_iterator & other) const noexcept
{
    throw not_implemented("bool allocator_boundary_tags::boundary_iterator::operator!=(const allocator_boundary_tags::boundary_iterator &) const noexcept", "your code should be here...");
}

allocator_boundary_tags::boundary_iterator &allocator_boundary_tags::boundary_iterator::operator++() & noexcept
{
    throw not_implemented("allocator_boundary_tags::boundary_iterator &allocator_boundary_tags::boundary_iterator::operator++() & noexcept", "your code should be here...");
}

allocator_boundary_tags::boundary_iterator &allocator_boundary_tags::boundary_iterator::operator--() & noexcept
{
    throw not_implemented("allocator_boundary_tags::boundary_iterator &allocator_boundary_tags::boundary_iterator::operator--() & noexcept", "your code should be here...");
}

allocator_boundary_tags::boundary_iterator allocator_boundary_tags::boundary_iterator::operator++(int n)
{
    throw not_implemented("allocator_boundary_tags::boundary_iterator allocator_boundary_tags::boundary_iterator::operator++(int n)", "your code should be here...");
}

allocator_boundary_tags::boundary_iterator allocator_boundary_tags::boundary_iterator::operator--(int n)
{
    throw not_implemented("allocator_boundary_tags::boundary_iterator allocator_boundary_tags::boundary_iterator::operator--(int n)", "your code should be here...");
}

size_t allocator_boundary_tags::boundary_iterator::size() const noexcept
{
    throw not_implemented("size_t allocator_boundary_tags::boundary_iterator::size() const noexcept", "your code should be here...");
}

bool allocator_boundary_tags::boundary_iterator::occupied() const noexcept
{
    throw not_implemented("bool allocator_boundary_tags::boundary_iterator::occupied() const noexcept", "your code should be here...");
}

void* allocator_boundary_tags::boundary_iterator::operator*() const noexcept
{
    throw not_implemented("void* allocator_boundary_tags::boundary_iterator::operator*() const noexcept", "your code should be here...");
}

allocator_boundary_tags::boundary_iterator::boundary_iterator()
{
    throw not_implemented("allocator_boundary_tags::boundary_iterator::boundary_iterator()", "your code should be here...");
}

allocator_boundary_tags::boundary_iterator::boundary_iterator(void *trusted)
{
    throw not_implemented("allocator_boundary_tags::boundary_iterator::boundary_iterator(void *)", "your code should be here...");
}

void *allocator_boundary_tags::boundary_iterator::get_ptr() const noexcept
{
    throw not_implemented("void *allocator_boundary_tags::boundary_iterator::get_ptr() const noexcept", "your code should be here...");
}
