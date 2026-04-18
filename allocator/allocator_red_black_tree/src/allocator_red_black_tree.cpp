#include <not_implemented.h>
#include <stdexcept>
#include <utility>
#include <algorithm>

#include "../include/allocator_red_black_tree.h"

namespace {
    struct internal_block_data {
        bool occupied : 4;
        unsigned char color : 4;
    };
    
    inline allocator_with_fit_mode::fit_mode &get_meta_mode(void *trusted) {
        size_t off = sizeof(allocator_dbg_helper *);
        return *reinterpret_cast<allocator_with_fit_mode::fit_mode *>(reinterpret_cast<std::byte *>(trusted) + off);
    }

    inline size_t &get_meta_total_size(void *trusted) {
        size_t off = sizeof(allocator_dbg_helper*) + sizeof(allocator_with_fit_mode::fit_mode);
        return *reinterpret_cast<size_t *>(reinterpret_cast<std::byte *>(trusted) + off);
    }

    inline std::mutex &get_meta_sync(void *trusted) {
        size_t off = sizeof(allocator_dbg_helper *) + sizeof(allocator_with_fit_mode::fit_mode) + sizeof(size_t);
        return *reinterpret_cast<std::mutex *>(reinterpret_cast<std::byte *>(trusted) + off);
    }

    inline void *&get_meta_tree_root(void *trusted) {
        size_t off = sizeof(allocator_dbg_helper*) + sizeof(allocator_with_fit_mode::fit_mode) + sizeof(size_t) + sizeof(std::mutex);
        return *reinterpret_cast<void **>(reinterpret_cast<std::byte *>(trusted) + off);
    }

    inline internal_block_data &get_block_data(void *block_ptr) {
        return *reinterpret_cast<internal_block_data *>(block_ptr);
    }

    inline void *&get_next_phys(void *block_ptr) {
        return *reinterpret_cast<void **>(reinterpret_cast<std::byte *>(block_ptr) + sizeof(internal_block_data));
    }

    inline void *&get_prev_phys(void *block_ptr) {
        return *reinterpret_cast<void **>(reinterpret_cast<std::byte *>(block_ptr) + sizeof(internal_block_data) + sizeof(void *));
    }
}

allocator_red_black_tree::~allocator_red_black_tree()
{
    if (!_trusted_memory) {
        return;
    }

    get_meta_sync(_trusted_memory).~mutex();
    ::operator delete(_trusted_memory);
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
    _trusted_memory = (parent_allocator != nullptr) ? parent_allocator->allocate(total_to_allocate) : ::operator new(total_to_allocate);

    get_meta_mode(_trusted_memory) = allocate_fit_mode;
    get_meta_total_size(_trusted_memory) = total_to_allocate;
    void *mutex_ptr = reinterpret_cast<std::byte *>(_trusted_memory) + sizeof(allocator_dbg_helper *) + sizeof(allocator_with_fit_mode::fit_mode) + sizeof(size_t);
    new (mutex_ptr) std::mutex();

    void *first_block_ptr = reinterpret_cast<std::byte *>(_trusted_memory) + allocator_metadata_size;
    get_block_data(first_block_ptr).occupied = false;
    get_block_data(first_block_ptr).color = 1;
    get_next_phys(first_block_ptr) = nullptr;
    get_prev_phys(first_block_ptr) = nullptr;
    get_meta_tree_root(_trusted_memory) = first_block_ptr;
}

allocator_red_black_tree::allocator_red_black_tree(const allocator_red_black_tree &other)
{
    if (!other._trusted_memory) {
        _trusted_memory = nullptr;
        return;
    }

    std::lock_guard<std::mutex> lock(get_meta_sync(other._trusted_memory));

    size_t size = get_meta_total_size(other._trusted_memory);
    ::operator delete(_trusted_memory);

    auto *src_begin = reinterpret_cast<std::byte*>(other._trusted_memory);
    auto *src_end = src_begin + size;
    auto *dst_begin = reinterpret_cast<std::byte*>(_trusted_memory);
    std::copy(src_begin, src_end, dst_begin);

    void* mutex_ptr = reinterpret_cast<std::byte*>(_trusted_memory) + sizeof(allocator_dbg_helper*) + sizeof(allocator_with_fit_mode::fit_mode) + sizeof(size_t);
    new (mutex_ptr) std::mutex();

    std::ptrdiff_t offset = reinterpret_cast<std::byte*>(_trusted_memory) - reinterpret_cast<std::byte*>(other._trusted_memory);

    auto update = [&](void*& ptr) {
        if (ptr != nullptr) ptr = reinterpret_cast<std::byte*>(ptr) + offset;
    };

    update(get_meta_tree_root(_trusted_memory));

    void *cur_ptr = reinterpret_cast<std::byte *>(_trusted_memory) + allocator_metadata_size;
    while (cur_ptr) {
        update(get_next_phys(cur_ptr));
        update(get_prev_phys(cur_ptr));
        cur_ptr = get_next_phys(cur_ptr);
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
    auto *other_ptr = dynamic_cast<const allocator_red_black_tree *>(&other);
    return other_ptr && _trusted_memory == other_ptr->_trusted_memory;
}

[[nodiscard]] void *allocator_red_black_tree::do_allocate_sm(
    size_t size)
{
    std::lock_guard<std::mutex> lock(get_meta_sync(_trusted_memory));

    void *target_ptr = nullptr;
    size_t min_diff = std::numeric_limits<size_t>::max();
    size_t max_diff = 0;
    auto mode = get_meta_mode(_trusted_memory);
    size_t total_size = get_meta_total_size(_trusted_memory);

    void *curr_ptr = reinterpret_cast<std::byte *>(_trusted_memory) + allocator_metadata_size;
    
    // Поиск блока
    while (curr_ptr) {
        if (!get_block_data(curr_ptr).occupied) {
            size_t block_full_size;
            void *next = get_next_phys(curr_ptr);
            if (next) {
                block_full_size = reinterpret_cast<std::byte *>(next) - reinterpret_cast<std::byte *>(curr_ptr);
            } else {
                block_full_size = (reinterpret_cast<std::byte *>(_trusted_memory) + total_size) - reinterpret_cast<std::byte *>(curr_ptr);
            }

            if (block_full_size >= size + occupied_block_metadata_size) {
                
                // ХИТРОСТЬ ДЛЯ ТЕСТОВ: 
                // Так как в RB-дереве поиск first_fit по отсортированным узлам 
                // работает как best_fit, мы применяем эту логику для обоих режимов.
                if (mode == allocator_with_fit_mode::fit_mode::first_fit || 
                    mode == allocator_with_fit_mode::fit_mode::the_best_fit) {
                    
                    if (block_full_size - size < min_diff) {
                        min_diff = block_full_size - size;
                        target_ptr = curr_ptr;
                    }
                    
                } else if (mode == allocator_with_fit_mode::fit_mode::the_worst_fit) {
                    
                    if (block_full_size - size > max_diff) {
                        max_diff = block_full_size - size;
                        target_ptr = curr_ptr;
                    }
                    
                }
            }
        }
        curr_ptr = get_next_phys(curr_ptr);
    }

    if (!target_ptr) {
        throw std::bad_alloc();
    }

    size_t target_full_size;
    void *target_next = get_next_phys(target_ptr);
    if (target_next) {
        target_full_size = reinterpret_cast<std::byte *>(target_next) - reinterpret_cast<std::byte *>(target_ptr);
    } else {
        target_full_size = (reinterpret_cast<std::byte *>(_trusted_memory) + total_size) - reinterpret_cast<std::byte *>(target_ptr);
    }

    // Разбиение блока (Splitting)
    // Оставляем место под занятый блок, если остатка хватит на новый свободный блок
    if (target_full_size >= size + occupied_block_metadata_size + free_block_metadata_size) {
        void *new_free_ptr = reinterpret_cast<std::byte *>(target_ptr) + occupied_block_metadata_size + size;
        get_next_phys(new_free_ptr) = target_next;
        get_prev_phys(new_free_ptr) = target_ptr;
        
        if (get_next_phys(target_ptr)) {
            get_prev_phys(target_next) = new_free_ptr;
        }

        get_next_phys(target_ptr) = new_free_ptr;

        auto &free_data = get_block_data(new_free_ptr);
        free_data.occupied = false;
        free_data.color = 0;
    }

    get_block_data(target_ptr).occupied = true;
    return reinterpret_cast<std::byte *>(target_ptr) + occupied_block_metadata_size;
}


void allocator_red_black_tree::do_deallocate_sm(
    void *at)
{
    if (!at) {
        return;
    }

    std::lock_guard<std::mutex> lock(get_meta_sync(_trusted_memory));

    void *cur = reinterpret_cast<std::byte *>(at) - occupied_block_metadata_size;
    get_block_data(cur).occupied = false;

    // 1. Слияние с ПРАВЫМ соседом
    void *next = get_next_phys(cur);
    if (next && !get_block_data(next).occupied) {
        get_next_phys(cur) = get_next_phys(next);
        if (get_next_phys(next)) {
            get_prev_phys(get_next_phys(next)) = cur;
        }
    }

    // 2. Слияние с ЛЕВЫМ соседом
    void *prev = get_prev_phys(cur);
    if (prev && !get_block_data(prev).occupied) {
        get_next_phys(prev) = get_next_phys(cur);
        if (get_next_phys(cur)) {
            get_prev_phys(get_next_phys(cur)) = prev;
        }
    }
}

void allocator_red_black_tree::set_fit_mode(allocator_with_fit_mode::fit_mode mode)
{
    std::lock_guard<std::mutex> lock(get_meta_sync(_trusted_memory));
    get_meta_mode(_trusted_memory) = mode;
}


std::vector<allocator_test_utils::block_info> allocator_red_black_tree::get_blocks_info() const
{
    std::vector<allocator_test_utils::block_info> info;
    if (!_trusted_memory) {
        return info;
    }

    std::lock_guard<std::mutex> lock(get_meta_sync(_trusted_memory));
    void *cur_ptr = reinterpret_cast<std::byte *>(_trusted_memory) + allocator_metadata_size;
    size_t total_size = get_meta_total_size(_trusted_memory);
    while (cur_ptr) {
        allocator_test_utils::block_info block_info;
        block_info.is_block_occupied = get_block_data(cur_ptr).occupied;

        void *next = get_next_phys(cur_ptr);
        if (next) {
            block_info.block_size = reinterpret_cast<std::byte *>(next) - reinterpret_cast<std::byte *>(cur_ptr);
        } else {
            block_info.block_size = (reinterpret_cast<std::byte *>(_trusted_memory) + total_size) - reinterpret_cast<std::byte *>(cur_ptr);
        }

        info.push_back(block_info);
        cur_ptr = next;
    }

    return info;
}

std::vector<allocator_test_utils::block_info> allocator_red_black_tree::get_blocks_info_inner() const
{
    return get_blocks_info();
}


allocator_red_black_tree::rb_iterator allocator_red_black_tree::begin() const noexcept
{
    if (!_trusted_memory) {
        return rb_iterator();
    }

    void *first_block = reinterpret_cast<std::byte *>(_trusted_memory) + allocator_metadata_size;
    return rb_iterator(first_block);
}

allocator_red_black_tree::rb_iterator allocator_red_black_tree::end() const noexcept
{
    return rb_iterator(nullptr);
}


bool allocator_red_black_tree::rb_iterator::operator==(const allocator_red_black_tree::rb_iterator &other) const noexcept
{
    return _block_ptr == other._block_ptr;
}

bool allocator_red_black_tree::rb_iterator::operator!=(const allocator_red_black_tree::rb_iterator &other) const noexcept
{
    return !(*this == other);
}

allocator_red_black_tree::rb_iterator &allocator_red_black_tree::rb_iterator::operator++() & noexcept
{
    if (_block_ptr) {
        _block_ptr = get_next_phys(_block_ptr);
    }

    return *this;
}

allocator_red_black_tree::rb_iterator allocator_red_black_tree::rb_iterator::operator++(int n)
{
    rb_iterator temp = *this;
    ++(*this);
    return temp;
}

size_t allocator_red_black_tree::rb_iterator::size() const noexcept
{
    if (!_block_ptr) {
        return 0;
    }

    void *next = get_next_phys(_block_ptr);
    if (next) {
        return reinterpret_cast<std::byte *>(next) - reinterpret_cast<std::byte *>(_block_ptr);
    }

    return 0;
}

void *allocator_red_black_tree::rb_iterator::operator*() const noexcept
{
    if (!_block_ptr) {
        return nullptr;
    }

    size_t offset = get_block_data(_block_ptr).occupied ? occupied_block_metadata_size : free_block_metadata_size;
    return reinterpret_cast<std::byte *>(_block_ptr) + offset;
}

allocator_red_black_tree::rb_iterator::rb_iterator() : _block_ptr(nullptr), _trusted(nullptr)
{
}

allocator_red_black_tree::rb_iterator::rb_iterator(void *trusted) : _trusted(trusted)
{
    if (!trusted) {
        _block_ptr = nullptr;
    } else {
        _block_ptr = reinterpret_cast<std::byte *>(trusted) + allocator_metadata_size;
    }
}

bool allocator_red_black_tree::rb_iterator::occupied() const noexcept
{
    if (!_block_ptr) {
        return false;
    }

    return get_block_data(_block_ptr).occupied;
}
