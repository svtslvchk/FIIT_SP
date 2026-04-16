#include <not_implemented.h>
#include <stdexcept>
#include <utility>
#include <algorithm>

#include "../include/allocator_red_black_tree.h"

namespace {

    enum class internal_color : unsigned char { RED, BLACK };

    struct internal_block_data {
        bool occupied : 4;
        internal_color color : 4;
    };

    struct allocator_metadata {
        allocator_dbg_helper* dbg;
        allocator_red_black_tree::fit_mode mode;
        size_t total_size;
        std::mutex sync;
        void* tree_root;
        std::pmr::memory_resource* parent;
    };

    struct free_block_meta {
        internal_block_data data;
        void* next_phys;
        void* prev_phys;
        void* left;
        void* right;
        void* parent;
    };

    struct occupied_block_meta {
        internal_block_data data;
        void* next_phys;
        void* prev_phys;
    };
}

allocator_red_black_tree::~allocator_red_black_tree()
{
    if (_trusted_memory == nullptr) {
        return;
    }

    auto *meta = reinterpret_cast<allocator_metadata *>(_trusted_memory);
    size_t total_size = meta->total_size;
    auto *parent = meta->parent;
    meta->sync.~mutex();

    if (parent != nullptr) {
        parent->deallocate(_trusted_memory, total_size);
    } else {
        ::operator delete(_trusted_memory);
    }

    _trusted_memory = nullptr;
}

allocator_red_black_tree::allocator_red_black_tree(
    allocator_red_black_tree &&other) noexcept
{
    _trusted_memory = other._trusted_memory;
    other._trusted_memory = nullptr;
}

allocator_red_black_tree &allocator_red_black_tree::operator=(
    allocator_red_black_tree &&other) noexcept
{
    if (this != &other) {
        this->~allocator_red_black_tree();
        _trusted_memory = other._trusted_memory;
        other._trusted_memory = nullptr;
    }

    return *this;
}

allocator_red_black_tree::allocator_red_black_tree(
        size_t space_size,
        std::pmr::memory_resource *parent_allocator,
        allocator_with_fit_mode::fit_mode allocate_fit_mode)
{
    if (space_size < free_block_metadata_size) {
        throw std::logic_error("Requested size is too small for RB-tree metadata");
    }

    size_t total_to_allocate = allocator_metadata_size + space_size;
    void *raw_ptr = (parent_allocator != nullptr)
        ? parent_allocator->allocate(total_to_allocate)
        : ::operator new(total_to_allocate);

    _trusted_memory = raw_ptr;

    auto *meta = reinterpret_cast<allocator_metadata *>(_trusted_memory);
    meta->dbg = nullptr; 
    meta->mode = allocate_fit_mode;
    meta->total_size = total_to_allocate;
    meta->parent = parent_allocator;
    new (&meta->sync) std::mutex();

    void *first_block_ptr = reinterpret_cast<std::byte *>(_trusted_memory) + allocator_metadata_size;
    auto *block = reinterpret_cast<free_block_meta *>(first_block_ptr);

    block->data.occupied = false;
    block->data.color = internal_color::BLACK;
    block->next_phys = nullptr;
    block->prev_phys = nullptr;
    block->left = nullptr;
    block->right = nullptr;
    block->parent = nullptr;

    meta->tree_root = first_block_ptr;
}

allocator_red_black_tree::allocator_red_black_tree(const allocator_red_black_tree &other)
{
    if (other._trusted_memory == nullptr) {
        _trusted_memory = nullptr;
        return;
    }

    auto *other_meta = reinterpret_cast<allocator_metadata *>(other._trusted_memory);
    std::lock_guard<std::mutex> lock(other_meta->sync);

    size_t size = other_meta->total_size;
    void *new_mem = (other_meta->parent != nullptr)
        ? other_meta->parent->allocate(size)
        : ::operator new(size);

    auto *src_begin = reinterpret_cast<std::byte*>(other._trusted_memory);
    auto *src_end = src_begin + size;
    auto *dst_begin = reinterpret_cast<std::byte*>(new_mem);
    std::copy(src_begin, src_end, dst_begin);
    _trusted_memory = new_mem;

    auto *meta = reinterpret_cast<allocator_metadata *>(_trusted_memory);
    new (&meta->sync) std::mutex(); // Мьютекс нельзя копировать

    ptrdiff_t offset = reinterpret_cast<std::byte*>(_trusted_memory) - reinterpret_cast<std::byte*>(other._trusted_memory);

    auto update_ptr = [&](void*& ptr) {
        if (ptr != nullptr) ptr = reinterpret_cast<std::byte*>(ptr) + offset;
    };

    update_ptr(meta->tree_root);

    void *curr_ptr = reinterpret_cast<std::byte *>(_trusted_memory) + allocator_metadata_size;
    while (curr_ptr != nullptr) {
        auto *base_block = reinterpret_cast<occupied_block_meta *>(curr_ptr);
        update_ptr(base_block->next_phys);
        update_ptr(base_block->prev_phys);

        if (!base_block->data.occupied) {
            auto *free_block = reinterpret_cast<free_block_meta *>(curr_ptr);
            update_ptr(free_block->left);
            update_ptr(free_block->right);
            update_ptr(free_block->parent);
        }

        curr_ptr = base_block->next_phys;
    }
}

allocator_red_black_tree &allocator_red_black_tree::operator=(const allocator_red_black_tree &other)
{
    if (this != &other) {
        allocator_red_black_tree temp = other;
        std::swap(_trusted_memory, temp._trusted_memory);
    }

    return *this;
}

bool allocator_red_black_tree::do_is_equal(const std::pmr::memory_resource &other) const noexcept
{
    throw not_implemented("bool allocator_red_black_tree::do_is_equal(const std::pmr::memory_resource &other) const noexcept", "your code should be here...");
}

[[nodiscard]] void *allocator_red_black_tree::do_allocate_sm(
    size_t size)
{
    throw not_implemented("[[nodiscard]] void *allocator_red_black_tree::do_allocate_sm(size_t)", "your code should be here...");
}


void allocator_red_black_tree::do_deallocate_sm(
    void *at)
{
    throw not_implemented("void allocator_red_black_tree::do_deallocate_sm(void *)", "your code should be here...");
}

void allocator_red_black_tree::set_fit_mode(allocator_with_fit_mode::fit_mode mode)
{
    throw not_implemented("void allocator_red_black_tree::set_fit_mode(allocator_with_fit_mode::fit_mode)", "your code should be here...");
}


std::vector<allocator_test_utils::block_info> allocator_red_black_tree::get_blocks_info() const
{
    throw not_implemented("std::vector<allocator_test_utils::block_info> allocator_red_black_tree::get_blocks_info() const", "your code should be here...");
}

std::vector<allocator_test_utils::block_info> allocator_red_black_tree::get_blocks_info_inner() const
{
    throw not_implemented("std::vector<allocator_test_utils::block_info> allocator_red_black_tree::get_blocks_info_inner() const", "your code should be here...");
}


allocator_red_black_tree::rb_iterator allocator_red_black_tree::begin() const noexcept
{
    throw not_implemented("allocator_red_black_tree::rb_iterator allocator_red_black_tree::begin() const noexcept", "your code should be here...");
}

allocator_red_black_tree::rb_iterator allocator_red_black_tree::end() const noexcept
{
    throw not_implemented("allocator_red_black_tree::rb_iterator allocator_red_black_tree::end() const noexcept", "your code should be here...");
}


bool allocator_red_black_tree::rb_iterator::operator==(const allocator_red_black_tree::rb_iterator &other) const noexcept
{
    throw not_implemented("bool allocator_red_black_tree::rb_iterator::operator==(const allocator_red_black_tree::rb_iterator &) const noexcept", "your code should be here...");
}

bool allocator_red_black_tree::rb_iterator::operator!=(const allocator_red_black_tree::rb_iterator &other) const noexcept
{
    throw not_implemented("bool allocator_red_black_tree::rb_iterator::operator!=(const allocator_red_black_tree::rb_iterator &) const noexcept", "your code should be here...");
}

allocator_red_black_tree::rb_iterator &allocator_red_black_tree::rb_iterator::operator++() & noexcept
{
    throw not_implemented("allocator_red_black_tree::rb_iterator &allocator_red_black_tree::rb_iterator::operator++() & noexcept", "your code should be here...");
}

allocator_red_black_tree::rb_iterator allocator_red_black_tree::rb_iterator::operator++(int n)
{
    throw not_implemented("allocator_red_black_tree::rb_iterator allocator_red_black_tree::rb_iterator::operator++(int)", "your code should be here...");
}

size_t allocator_red_black_tree::rb_iterator::size() const noexcept
{
    throw not_implemented("size_t allocator_red_black_tree::rb_iterator::size() const noexcept", "your code should be here...");
}

void *allocator_red_black_tree::rb_iterator::operator*() const noexcept
{
    throw not_implemented("void *allocator_red_black_tree::rb_iterator::operator*() const noexcept", "your code should be here...");
}

allocator_red_black_tree::rb_iterator::rb_iterator()
{
    throw not_implemented("allocator_red_black_tree::rb_iterator::rb_iterator()", "your code should be here...");
}

allocator_red_black_tree::rb_iterator::rb_iterator(void *trusted)
{
    throw not_implemented("allocator_red_black_tree::rb_iterator::rb_iterator(void *)", "your code should be here...");
}

bool allocator_red_black_tree::rb_iterator::occupied() const noexcept
{
    throw not_implemented("bool allocator_red_black_tree::rb_iterator::occupied() const noexcept", "your code should be here...");
}
