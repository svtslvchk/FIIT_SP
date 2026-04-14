#include <not_implemented.h>
#include <cstddef>
#include <new>
#include "../include/allocator_buddies_system.h"

struct allocator_metadata {
    std::pmr::memory_resource *parent_allocator;
    allocator_dbg_helper *dgb_helper;
    allocator_with_fit_mode::fit_mode fit_mode;
    unsigned char size_power; // размер степень двойки
    std::mutex sync;
};

allocator_buddies_system::~allocator_buddies_system()
{
    if (!_trusted_memory) {
        return;
    }

    auto *meta = reinterpret_cast<allocator_metadata *>(_trusted_memory);
    std::pmr::memory_resource *parent = meta->parent_allocator;
    size_t total_size = sizeof(allocator_metadata) + (1ULL << meta->size_power);

    meta->sync.~mutex();

    if (parent) {
        parent->deallocate(_trusted_memory, total_size);
    } else {
        :: operator delete(_trusted_memory);
    }
}

allocator_buddies_system::allocator_buddies_system(
    allocator_buddies_system &&other) noexcept : _trusted_memory(other._trusted_memory)
{
    other._trusted_memory = nullptr;
}

allocator_buddies_system &allocator_buddies_system::operator=(
    allocator_buddies_system &&other) noexcept
{
    if (this != &other) {
        this->~allocator_buddies_system();
        _trusted_memory = other._trusted_memory;
        other._trusted_memory = nullptr;
    }

    return *this;
}

allocator_buddies_system::allocator_buddies_system(
        size_t space_size,
        std::pmr::memory_resource *parent_allocator,
        allocator_with_fit_mode::fit_mode allocate_fit_mode)
{
    size_t total_size = sizeof(allocator_metadata) + (1ULL << space_size);
    void *raw_mem = (parent_allocator != nullptr) 
        ? parent_allocator->allocate(total_size) 
        : ::operator new(total_size);

    _trusted_memory = raw_mem;
    auto *meta = new (_trusted_memory) allocator_metadata {
        parent_allocator,
        nullptr,
        allocate_fit_mode,
        static_cast<unsigned char>(space_size)
    };

    auto *first_block = reinterpret_cast<block_metadata *> (
        static_cast<std::byte *>(_trusted_memory) + sizeof(allocator_metadata)
    );

    first_block->occupied = false;
    first_block->size = static_cast<unsigned char>(space_size);
}

inline size_t get_relative_offset(void *block_ptr, void *trusted_memory) {
    std::byte *data_start = reinterpret_cast<std::byte *>(trusted_memory) + sizeof(allocator_metadata);
    return reinterpret_cast<std::byte *>(block_ptr) - data_start;
}

inline void* get_buddy_ptr(void *block_ptr, unsigned char power, void *trusted_memory) {
    // 1. Находим смещение текущего блока относительно начала области данных
    size_t relative_offset = get_relative_offset(block_ptr, trusted_memory);
    
    // 2. Считаем размер блока в байтах (2^power)
    size_t block_size = 1ULL << power;
    
    // 3. Магия XOR: инвертируем бит, отвечающий за позицию двойника
    size_t buddy_relative_offset = relative_offset ^ block_size;
    
    // 4. Возвращаем абсолютный адрес: начало данных + новое смещение
    std::byte *data_start = reinterpret_cast<std::byte *>(trusted_memory) + sizeof(allocator_metadata);
    return data_start + buddy_relative_offset;
}

[[nodiscard]] void *allocator_buddies_system::do_allocate_sm(
    size_t size)
{
    auto *meta = reinterpret_cast<allocator_metadata *>(_trusted_memory);
    std::lock_guard<std::mutex> lock(meta->sync);
    size_t total_size = size +occupied_block_metadata_size;
    unsigned char target_power = static_cast<unsigned char>(__detail::nearest_greater_k_of_2(total_size));
    if (target_power < min_k) {
        target_power = min_k;
    }

    void *found_block_start = nullptr;
    size_t found_block_size = 0;
    for (auto i = begin(); i != end(); i++) {
        void *cur_block_start = reinterpret_cast<std::byte *>(*i) - sizeof(block_metadata);
        auto *cur_meta = reinterpret_cast<block_metadata *>(cur_block_start);

        if (!cur_meta->occupied && cur_meta->size >= target_power) {
            size_t cur_sz = 1ULL << cur_meta->size;

            if (meta->fit_mode == fit_mode::first_fit) {
                found_block_start = cur_block_start;
                break;
            } else if (meta->fit_mode == fit_mode::the_best_fit) {
                if (!found_block_start || cur_sz < found_block_size) {
                    found_block_start = cur_block_start;
                    found_block_size = cur_sz;
                }
            } else if (meta->fit_mode == fit_mode::the_worst_fit) {
                if (!found_block_start || cur_sz > found_block_size) {
                    found_block_start = cur_block_start;
                    found_block_size = cur_sz;
                }
            }
        }
    }

    if (!found_block_size) {
        throw std::bad_alloc();
    }

    auto *target_block_meta = reinterpret_cast<block_metadata *>(found_block_start);

    while (target_block_meta->size > target_power) {
        target_block_meta->size -= 1;
        void *buddy_start = get_buddy_ptr(found_block_start, target_block_meta->size, _trusted_memory);
        auto *buddy_meta = reinterpret_cast<block_metadata *>(buddy_start);
        buddy_meta->occupied = false;
        buddy_meta->size = target_block_meta->size;
    }

    target_block_meta->occupied = true;

    void *allocator_ptr_pos = reinterpret_cast<std::byte *>(found_block_start) + sizeof(block_metadata);
    *reinterpret_cast<void **>(allocator_ptr_pos) = this;

    return reinterpret_cast<std::byte *>(found_block_start) + occupied_block_metadata_size;
}

void allocator_buddies_system::do_deallocate_sm(void *at)
{
    throw not_implemented("void allocator_buddies_system::do_deallocate_sm(void *)", "your code should be here...");
}

allocator_buddies_system::allocator_buddies_system(const allocator_buddies_system &other)
{
    if (!other._trusted_memory) {
        _trusted_memory = nullptr;
        return;
    }

    // Извлекаем настройки из другого аллокатора
    // Нам нужно заблокировать мьютекс оригинала, чтобы безопасно прочитать настройки
    auto *other_meta = reinterpret_cast<allocator_metadata *>(other._trusted_memory);
    
    size_t power;
    std::pmr::memory_resource *parent;
    allocator_with_fit_mode::fit_mode mode;

    {
        std::lock_guard<std::mutex> lock(other_meta->sync);
        power = other_meta->size_power;
        parent = other_meta->parent_allocator;
        mode = other_meta->fit_mode;
    }

    size_t total_size = sizeof(allocator_metadata) + (1ULL << power);

    void *raw_mem = (parent != nullptr) 
        ? parent->allocate(total_size) 
        : ::operator new(total_size);

    _trusted_memory = raw_mem;

    new (_trusted_memory) allocator_metadata {
        parent,
        nullptr,
        mode,
        static_cast<unsigned char>(power)
    };

    auto *first_block = reinterpret_cast<block_metadata *>(
        reinterpret_cast<std::byte *>(_trusted_memory) + sizeof(allocator_metadata)
    );

    first_block->occupied = false;
    first_block->size = static_cast<unsigned char>(power);
}

allocator_buddies_system &allocator_buddies_system::operator=(const allocator_buddies_system &other)
{
    if (this != &other) {
        allocator_buddies_system temp(other);
        std::swap(_trusted_memory, temp._trusted_memory);
    }

    return *this;
}

bool allocator_buddies_system::do_is_equal(const std::pmr::memory_resource &other) const noexcept
{
    return this == &other;
}

inline void allocator_buddies_system::set_fit_mode(
    allocator_with_fit_mode::fit_mode mode)
{
    auto *meta = reinterpret_cast<allocator_metadata *>(_trusted_memory);
    std::lock_guard<std::mutex> lock(meta->sync);
    meta->fit_mode = mode;
}


std::vector<allocator_test_utils::block_info> allocator_buddies_system::get_blocks_info() const noexcept
{
    auto *meta = reinterpret_cast<allocator_metadata *>(_trusted_memory);
    std::lock_guard<std::mutex> lock(meta->sync);
    try {
        return get_blocks_info_inner();
    } catch (...) {
        return {};
    }
}

std::vector<allocator_test_utils::block_info> allocator_buddies_system::get_blocks_info_inner() const
{
    std::vector<allocator_test_utils::block_info> info;
    for (auto i = begin(); i != end(); i++) {
        info.push_back({i.size(), i.occupied()});
    }

    return info;
}

allocator_buddies_system::buddy_iterator allocator_buddies_system::begin() const noexcept
{
    return buddy_iterator(reinterpret_cast<std::byte *>(_trusted_memory) + sizeof(allocator_metadata));
}

allocator_buddies_system::buddy_iterator allocator_buddies_system::end() const noexcept
{
    auto *meta = reinterpret_cast<allocator_metadata *>(_trusted_memory);
    return buddy_iterator(reinterpret_cast<std::byte *>(_trusted_memory) + sizeof(allocator_metadata) + (1ULL << meta->size_power));
}

bool allocator_buddies_system::buddy_iterator::operator==(const allocator_buddies_system::buddy_iterator &other) const noexcept
{
    return _block == other._block;
}

bool allocator_buddies_system::buddy_iterator::operator!=(const allocator_buddies_system::buddy_iterator &other) const noexcept
{
    return !(*this == other);
}

allocator_buddies_system::buddy_iterator &allocator_buddies_system::buddy_iterator::operator++() & noexcept
{
    if (_block) {
        auto *meta = reinterpret_cast<block_metadata *>(_block);
        _block = reinterpret_cast<std::byte *>(_block) + (1ULL << meta->size);
    }

    return *this;
}

allocator_buddies_system::buddy_iterator allocator_buddies_system::buddy_iterator::operator++(int n)
{
    buddy_iterator temp = *this;
    ++(*this);
    return temp;
}

size_t allocator_buddies_system::buddy_iterator::size() const noexcept
{
    return 1ULL << reinterpret_cast<block_metadata *>(_block)->size;
}

bool allocator_buddies_system::buddy_iterator::occupied() const noexcept
{
    return reinterpret_cast<block_metadata *>(_block)->occupied;
}

void *allocator_buddies_system::buddy_iterator::operator*() const noexcept
{
    return reinterpret_cast<std::byte *>(_block) + sizeof(block_metadata);
}

allocator_buddies_system::buddy_iterator::buddy_iterator(void *start) : _block(start)
{
}

allocator_buddies_system::buddy_iterator::buddy_iterator() : _block(nullptr)
{
}
