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

[[nodiscard]] void *allocator_buddies_system::do_allocate_sm(
    size_t size)
{
    throw not_implemented("[[nodiscard]] void *allocator_buddies_system::do_allocate_sm(size_t)", "your code should be here...");
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
    throw not_implemented("bool allocator_buddies_system::do_is_equal(const std::pmr::memory_resource &other) const noexcept", "your code should be here...");
}

inline void allocator_buddies_system::set_fit_mode(
    allocator_with_fit_mode::fit_mode mode)
{
    throw not_implemented("inline void allocator_buddies_system::set_fit_mode(allocator_with_fit_mode::fit_mode)", "your code should be here...");
}


std::vector<allocator_test_utils::block_info> allocator_buddies_system::get_blocks_info() const noexcept
{
    throw not_implemented("std::vector<allocator_test_utils::block_info> allocator_buddies_system::get_blocks_info() const noexcept", "your code should be here...");
}

std::vector<allocator_test_utils::block_info> allocator_buddies_system::get_blocks_info_inner() const
{
    throw not_implemented("std::vector<allocator_test_utils::block_info> allocator_buddies_system::get_blocks_info_inner() const", "your code should be here...");
}

allocator_buddies_system::buddy_iterator allocator_buddies_system::begin() const noexcept
{
    throw not_implemented("allocator_buddies_system::buddy_iterator allocator_buddies_system::begin() const noexcept", "your code should be here...");
}

allocator_buddies_system::buddy_iterator allocator_buddies_system::end() const noexcept
{
    throw not_implemented("allocator_buddies_system::buddy_iterator allocator_buddies_system::end() const noexcept", "your code should be here...");
}

bool allocator_buddies_system::buddy_iterator::operator==(const allocator_buddies_system::buddy_iterator &other) const noexcept
{
    throw not_implemented("bool allocator_buddies_system::buddy_iterator::operator==(const allocator_buddies_system::buddy_iterator &) const noexcept", "your code should be here...");
}

bool allocator_buddies_system::buddy_iterator::operator!=(const allocator_buddies_system::buddy_iterator &other) const noexcept
{
    throw not_implemented("bool allocator_buddies_system::buddy_iterator::operator!=(const allocator_buddies_system::buddy_iterator &) const noexcept", "your code should be here...");
}

allocator_buddies_system::buddy_iterator &allocator_buddies_system::buddy_iterator::operator++() & noexcept
{
    throw not_implemented("allocator_buddies_system::buddy_iterator &allocator_buddies_system::buddy_iterator::operator++() & noexcept", "your code should be here...");
}

allocator_buddies_system::buddy_iterator allocator_buddies_system::buddy_iterator::operator++(int n)
{
    throw not_implemented("allocator_buddies_system::buddy_iterator allocator_buddies_system::buddy_iterator::operator++(int)", "your code should be here...");
}

size_t allocator_buddies_system::buddy_iterator::size() const noexcept
{
    throw not_implemented("size_t allocator_buddies_system::buddy_iterator::size() const noexcept", "your code should be here...");
}

bool allocator_buddies_system::buddy_iterator::occupied() const noexcept
{
    throw not_implemented("bool allocator_buddies_system::buddy_iterator::occupied() const noexcept", "your code should be here...");
}

void *allocator_buddies_system::buddy_iterator::operator*() const noexcept
{
    throw not_implemented("void *allocator_buddies_system::buddy_iterator::operator*() const noexcept", "your code should be here...");
}

allocator_buddies_system::buddy_iterator::buddy_iterator(void *start)
{
    throw not_implemented("allocator_buddies_system::buddy_iterator::buddy_iterator(void *)", "your code should be here...");
}

allocator_buddies_system::buddy_iterator::buddy_iterator()
{
    throw not_implemented("allocator_buddies_system::buddy_iterator::buddy_iterator()", "your code should be here...");
}
