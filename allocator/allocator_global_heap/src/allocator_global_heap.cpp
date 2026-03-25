#include <new>
#include <limits> 
#include "../include/allocator_global_heap.h"

allocator_global_heap::allocator_global_heap(): smart_mem_resource(), allocator_dbg_helper()
{
}

[[nodiscard]] void *allocator_global_heap::do_allocate_sm(
    size_t size)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!size) {
        return nullptr;
    }

    if (size > std::numeric_limits<size_t>::max()) {
        throw std::bad_alloc();
    }

    void *ptr = ::operator new(size, std::nothrow);
    if (!ptr) {
        throw std::bad_alloc();
    }

    return ptr;
}

void allocator_global_heap::do_deallocate_sm(
    void *at)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!at) {
        return;
    }

    ::operator delete(at, std::nothrow);
}

allocator_global_heap::~allocator_global_heap()
{
}

allocator_global_heap::allocator_global_heap(const allocator_global_heap &other): 
smart_mem_resource(other), allocator_dbg_helper(other), m_mutex() 
{
}

allocator_global_heap &allocator_global_heap::operator=(const allocator_global_heap &other)
{
    if (this != &other) {
        smart_mem_resource::operator=(other);
        allocator_dbg_helper::operator=(other);
    }

    return *this;
}

bool allocator_global_heap::do_is_equal(const std::pmr::memory_resource &other) const noexcept
{
    const auto* other_alloc = dynamic_cast<const allocator_global_heap*>(&other);
    return other_alloc != nullptr;
}

allocator_global_heap::allocator_global_heap(allocator_global_heap &&other) noexcept:
smart_mem_resource(std::move(other)), allocator_dbg_helper(std::move(other)), m_mutex()
{
}

allocator_global_heap &allocator_global_heap::operator=(allocator_global_heap &&other) noexcept
{
    if (this != &other) {
        smart_mem_resource::operator=(std::move(other));
        allocator_dbg_helper::operator=(std::move(other));
    }

    return *this;
}
