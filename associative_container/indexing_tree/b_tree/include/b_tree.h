#ifndef SYS_PROG_B_TREE_H
#define SYS_PROG_B_TREE_H

#include "../../../include/associative_container.h"

#include <iterator>
#include <utility>
#include <boost/container/static_vector.hpp>
#include <stack>
#include <pp_allocator.h>
#include <associative_container.h>
#include <not_implemented.h>
#include <initializer_list> 
#include <cstddef>

template <typename tkey, typename tvalue, comparator<tkey> compare = std::less<tkey>, std::size_t t = 5>
class B_tree final : private compare // EBCO
{
public:

    using tree_data_type = std::pair<tkey, tvalue>;
    using tree_data_type_const = std::pair<const tkey, tvalue>;
    using value_type = tree_data_type_const;

private:

    static constexpr const size_t minimum_keys_in_node = t - 1;
    static constexpr const size_t maximum_keys_in_node = 2 * t - 1;

    // region comparators declaration

    inline bool compare_keys(const tkey& lhs, const tkey& rhs) const;
    inline bool compare_pairs(const tree_data_type& lhs, const tree_data_type& rhs) const;

    // endregion comparators declaration


    struct btree_node
    {
        boost::container::static_vector<tree_data_type, maximum_keys_in_node + 1> _keys;
        boost::container::static_vector<btree_node*, maximum_keys_in_node + 2> _pointers;
        btree_node() noexcept;
    };

    pp_allocator<value_type> _allocator;
    btree_node* _root;
    size_t _size;

    pp_allocator<value_type> get_allocator() const noexcept;

    btree_node* copy_tree(btree_node* other_node);

    void r_clear(btree_node *node) noexcept;

    void split_child(btree_node *parent, std::size_t index);

    void insert_non_full(btree_node *node, tree_data_type &&data);

    void remove_recursive(btree_node* node, const tkey& key);

    void handle_internal_node_delete(btree_node* node, std::size_t idx);

    void fill_child(btree_node* parent, std::size_t idx);

    void borrow_from_prev(btree_node* parent, std::size_t idx);

    void borrow_from_next(btree_node* parent, std::size_t idx);

    void merge_nodes(btree_node* parent, std::size_t idx);

    tree_data_type get_predecessor(btree_node* node);

    tree_data_type get_successor(btree_node* node);

public:

    // region constructors declaration

    explicit B_tree(const compare& cmp = compare(), pp_allocator<value_type> = pp_allocator<value_type>());

    explicit B_tree(pp_allocator<value_type> alloc, const compare& comp = compare());

    template<input_iterator_for_pair<tkey, tvalue> iterator>
    explicit B_tree(iterator begin, iterator end, const compare& cmp = compare(), pp_allocator<value_type> = pp_allocator<value_type>());

    B_tree(std::initializer_list<std::pair<tkey, tvalue>> data, const compare& cmp = compare(), pp_allocator<value_type> = pp_allocator<value_type>());

    // endregion constructors declaration

    // region five declaration

    B_tree(const B_tree& other);

    B_tree(B_tree&& other) noexcept;

    B_tree& operator=(const B_tree& other);

    B_tree& operator=(B_tree&& other) noexcept;

    ~B_tree() noexcept;

    // endregion five declaration

    // region iterators declaration

    class btree_iterator;
    class btree_reverse_iterator;
    class btree_const_iterator;
    class btree_const_reverse_iterator;

    class btree_iterator final
    {
        std::stack<std::pair<btree_node**, size_t>> _path;
        size_t _index;

    public:
        using value_type = tree_data_type_const;
        using reference = value_type&;
        using pointer = value_type*;
        using iterator_category = std::bidirectional_iterator_tag;
        using difference_type = ptrdiff_t;
        using self = btree_iterator;

        friend class B_tree;
        friend class btree_reverse_iterator;
        friend class btree_const_iterator;
        friend class btree_const_reverse_iterator;

        reference operator*() const noexcept;
        pointer operator->() const noexcept;

        self& operator++();
        self operator++(int);

        self& operator--();
        self operator--(int);

        bool operator==(const self& other) const noexcept;
        bool operator!=(const self& other) const noexcept;

        size_t depth() const noexcept;
        size_t current_node_keys_count() const noexcept;
        bool is_terminate_node() const noexcept;
        size_t index() const noexcept;

        explicit btree_iterator(const std::stack<std::pair<btree_node**, size_t>>& path = std::stack<std::pair<btree_node**, size_t>>(), size_t index = 0);

    };

    class btree_const_iterator final
    {
        std::stack<std::pair<btree_node* const*, size_t>> _path;
        size_t _index;

    public:

        size_t debug_get_level() const { 
            return _path.size() > 0 ? _path.size() - 1 : 0; 
        }
        
        size_t debug_get_index() const { 
            return _index; 
        }

        using value_type = tree_data_type_const;
        using reference = const value_type&;
        using pointer = const value_type*;
        using iterator_category = std::bidirectional_iterator_tag;
        using difference_type = ptrdiff_t;
        using self = btree_const_iterator;

        friend class B_tree;
        friend class btree_reverse_iterator;
        friend class btree_iterator;
        friend class btree_const_reverse_iterator;

        btree_const_iterator(const btree_iterator& it) noexcept;

        reference operator*() const noexcept;
        pointer operator->() const noexcept;

        self& operator++();
        self operator++(int);

        self& operator--();
        self operator--(int);

        bool operator==(const self& other) const noexcept;
        bool operator!=(const self& other) const noexcept;

        size_t depth() const noexcept;
        size_t current_node_keys_count() const noexcept;
        bool is_terminate_node() const noexcept;
        size_t index() const noexcept;

        explicit btree_const_iterator(const std::stack<std::pair<btree_node* const*, size_t>>& path = std::stack<std::pair<btree_node* const*, size_t>>(), size_t index = 0);
    };

    class btree_reverse_iterator final
    {
        std::stack<std::pair<btree_node**, size_t>> _path;
        size_t _index;

    public:

        using value_type = tree_data_type_const;
        using reference = value_type&;
        using pointer = value_type*;
        using iterator_category = std::bidirectional_iterator_tag;
        using difference_type = ptrdiff_t;
        using self = btree_reverse_iterator;

        friend class B_tree;
        friend class btree_iterator;
        friend class btree_const_iterator;
        friend class btree_const_reverse_iterator;

        btree_reverse_iterator(const btree_iterator& it) noexcept;
        operator btree_iterator() const noexcept;

        reference operator*() const noexcept;
        pointer operator->() const noexcept;

        self& operator++();
        self operator++(int);

        self& operator--();
        self operator--(int);

        bool operator==(const self& other) const noexcept;
        bool operator!=(const self& other) const noexcept;

        size_t depth() const noexcept;
        size_t current_node_keys_count() const noexcept;
        bool is_terminate_node() const noexcept;
        size_t index() const noexcept;

        explicit btree_reverse_iterator(const std::stack<std::pair<btree_node**, size_t>>& path = std::stack<std::pair<btree_node**, size_t>>(), size_t index = 0);
    };

    class btree_const_reverse_iterator final
    {
        std::stack<std::pair<btree_node* const*, size_t>> _path;
        size_t _index;

    public:

        using value_type = tree_data_type_const;
        using reference = const value_type&;
        using pointer = const value_type*;
        using iterator_category = std::bidirectional_iterator_tag;
        using difference_type = ptrdiff_t;
        using self = btree_const_reverse_iterator;

        friend class B_tree;
        friend class btree_reverse_iterator;
        friend class btree_const_iterator;
        friend class btree_iterator;

        btree_const_reverse_iterator(const btree_reverse_iterator& it) noexcept;
        operator btree_const_iterator() const noexcept;

        reference operator*() const noexcept;
        pointer operator->() const noexcept;

        self& operator++();
        self operator++(int);

        self& operator--();
        self operator--(int);

        bool operator==(const self& other) const noexcept;
        bool operator!=(const self& other) const noexcept;

        size_t depth() const noexcept;
        size_t current_node_keys_count() const noexcept;
        bool is_terminate_node() const noexcept;
        size_t index() const noexcept;

        explicit btree_const_reverse_iterator(const std::stack<std::pair<btree_node* const*, size_t>>& path = std::stack<std::pair<btree_node* const*, size_t>>(), size_t index = 0);
    };

    friend class btree_iterator;
    friend class btree_const_iterator;
    friend class btree_reverse_iterator;
    friend class btree_const_reverse_iterator;

    // endregion iterators declaration

    // region element access declaration

    /*
     * Returns a reference to the mapped value of the element with specified key. If no such element exists, an exception of type std::out_of_range is thrown.
     */
    tvalue& at(const tkey&);
    const tvalue& at(const tkey&) const;

    /*
     * If key not exists, makes default initialization of value
     */
    tvalue& operator[](const tkey& key);
    tvalue& operator[](tkey&& key);

    // endregion element access declaration
    // region iterator begins declaration

    btree_iterator begin();
    btree_iterator end();

    btree_const_iterator begin() const;
    btree_const_iterator end() const;

    btree_const_iterator cbegin() const;
    btree_const_iterator cend() const;

    btree_reverse_iterator rbegin();
    btree_reverse_iterator rend();

    btree_const_reverse_iterator rbegin() const;
    btree_const_reverse_iterator rend() const;

    btree_const_reverse_iterator crbegin() const;
    btree_const_reverse_iterator crend() const;

    // endregion iterator begins declaration

    // region lookup declaration

    size_t size() const noexcept;
    bool empty() const noexcept;

    /*
     * Returns end() if not exist
     */

    btree_iterator find(const tkey& key);
    btree_const_iterator find(const tkey& key) const;

    btree_iterator lower_bound(const tkey& key);
    btree_const_iterator lower_bound(const tkey& key) const;

    btree_iterator upper_bound(const tkey& key);
    btree_const_iterator upper_bound(const tkey& key) const;

    bool contains(const tkey& key) const;

    // endregion lookup declaration

    // region modifiers declaration

    void clear() noexcept;

    /*
     * Does nothing if key exists, delegates to emplace.
     * Second return value is true, when inserted
     */
    std::pair<btree_iterator, bool> insert(const tree_data_type& data);
    std::pair<btree_iterator, bool> insert(tree_data_type&& data);

    template <typename ...Args>
    std::pair<btree_iterator, bool> emplace(Args&&... args);

    /*
     * Updates value if key exists, delegates to emplace.
     */
    btree_iterator insert_or_assign(const tree_data_type& data);
    btree_iterator insert_or_assign(tree_data_type&& data);

    template <typename ...Args>
    btree_iterator emplace_or_assign(Args&&... args);

    /*
     * Return iterator to node next ro removed or end() if key not exists
     */
    btree_iterator erase(btree_iterator pos);
    btree_iterator erase(btree_const_iterator pos);

    btree_iterator erase(btree_iterator beg, btree_iterator en);
    btree_iterator erase(btree_const_iterator beg, btree_const_iterator en);


    btree_iterator erase(const tkey& key);

    // endregion modifiers declaration
};

template<std::input_iterator iterator, comparator<typename std::iterator_traits<iterator>::value_type::first_type> compare = std::less<typename std::iterator_traits<iterator>::value_type::first_type>,
        std::size_t t = 5, typename U>
B_tree(iterator begin, iterator end, const compare &cmp = compare(), pp_allocator<U> = pp_allocator<U>()) -> B_tree<typename std::iterator_traits<iterator>::value_type::first_type, typename std::iterator_traits<iterator>::value_type::second_type, compare, t>;

template<typename tkey, typename tvalue, comparator<tkey> compare = std::less<tkey>, std::size_t t = 5, typename U>
B_tree(std::initializer_list<std::pair<tkey, tvalue>> data, const compare &cmp = compare(), pp_allocator<U> = pp_allocator<U>()) -> B_tree<tkey, tvalue, compare, t>;

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::compare_pairs(const B_tree::tree_data_type &lhs,
                                                     const B_tree::tree_data_type &rhs) const
{
    return compare_keys(lhs.first, rhs.first);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::compare_keys(const tkey &lhs, const tkey &rhs) const
{
    return compare::operator()(lhs, rhs);
}


template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::btree_node::btree_node() noexcept
{
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
pp_allocator<typename B_tree<tkey, tvalue, compare, t>::value_type> B_tree<tkey, tvalue, compare, t>::get_allocator() const noexcept
{
    return _allocator;
}

// region constructors implementation

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::B_tree(
        const compare& cmp,
        pp_allocator<value_type> alloc) : compare(cmp), _allocator(alloc), _root(nullptr), _size(0)
{
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::B_tree(
        pp_allocator<value_type> alloc,\
        const compare& comp) : B_tree(comp, alloc)
{
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
template<input_iterator_for_pair<tkey, tvalue> iterator>
B_tree<tkey, tvalue, compare, t>::B_tree(
        iterator begin,
        iterator end,
        const compare& cmp,
        pp_allocator<value_type> alloc) : B_tree(cmp, alloc)
{
    for (auto i = begin; i != end; i++) {
        insert(*i);
    }
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::B_tree(
        std::initializer_list<std::pair<tkey, tvalue>> data,
        const compare& cmp,
        pp_allocator<value_type> alloc) : B_tree(data.begin(), data.end(), cmp, alloc)
{
}

// endregion constructors implementation

// region five implementation

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::~B_tree() noexcept
{
    clear();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_node* B_tree<tkey, tvalue, compare, t>::copy_tree(btree_node* other_node)
{
    if (!other_node) {
        return nullptr;
    }

    btree_node *new_node = new btree_node();
    new_node->_keys = other_node->_keys;

    for (auto child : other_node->_pointers) {
        new_node->_pointers.push_back(copy_tree(child));
    }

    return new_node;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::B_tree(const B_tree& other) : compare(other), _allocator(other._allocator), _size(other._size)
{
    _root = copy_tree(other._root);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>& B_tree<tkey, tvalue, compare, t>::operator=(const B_tree& other)
{
    if (this != &other) {
        B_tree tmp(other);
        std::swap(_root, tmp._root);
        std::swap(_size, tmp._size);
    }

    return *this;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::B_tree(B_tree&& other) noexcept
    : compare(std::move(other)), _allocator(std::move(other._allocator)), _root(other._root), _size(other._size)
{
    other._root = nullptr;
    other._size = 0;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>& B_tree<tkey, tvalue, compare, t>::operator=(B_tree&& other) noexcept
{
    if (this != &other) {
        this->clear();
        compare::operator=(std::move(other));
        this->_root = other._root;
        this->_allocator = std::move(other._allocator);
        other._root = nullptr;
        other._size = 0;
    }

    return *this;
}

// endregion five implementation

// region iterators implementation

namespace btree_details {

    template<typename RefType, typename NodeType>
    RefType get_node_data(const std::pair<NodeType *, size_t> &top) {
        return reinterpret_cast<RefType>((*top.first)->_keys[top.second]);
    }

    template<typename NodePtrType>
    void increment_logic(std::stack<std::pair<NodePtrType* const*, size_t>> &path, size_t &index) {
        if (path.empty()) {
            return;
        }

        auto *node = *(path.top().first);

        if (!node->_pointers.empty() && index + 1 < node->_pointers.size()) {
            path.top().second = index + 1;
            auto* const* child_ptr = &(node->_pointers[index + 1]);
            while (child_ptr && *child_ptr) {
                path.push({child_ptr, 0});
                auto *child_node = *child_ptr;
                if (child_node->_pointers.empty()) {
                    break;
                }

                child_ptr = &(child_node->_pointers[0]);
            }

            index = 0;
        } else if (index + 1 < node->_keys.size()) {
            index++;
            path.top().second = index;
        } else {
            while (true) {
                path.pop();
                if (path.empty()) {
                    index = 0;
                    return;
                }

                auto *parent_node = *(path.top().first);
                if (path.top().second < parent_node->_keys.size()) {
                    index = path.top().second;
                    return;
                }
            }
        }
    }

    template<typename NodePtrType>
    void decrement_logic(std::stack<std::pair<NodePtrType* const*, size_t>> &path, size_t &index) {
        if (path.empty()) {
            return;
        }

        auto *node = *(path.top().first);

        if (!node->_pointers.empty()) {
            path.top().second = index;
            auto* const* child_ptr = &(node->_pointers[index]);
            while (child_ptr && *child_ptr) {
                auto* child_node = *child_ptr;
                size_t child_index = child_node->_pointers.empty() ? 0 : child_node->_pointers.size() - 1;
                path.push({child_ptr, child_index});
                if (child_node->_pointers.empty()) {
                    index = child_node->_keys.size() - 1;
                    break;
                }
                child_ptr = &(child_node->_pointers[child_node->_pointers.size() - 1]);
            }
        } 
        // 2. Если мы в листе и это не самый первый ключ
        else if (index > 0) {
            index--;
        } 
        // 3. Подъем вверх
        else {
            while (true) {
                path.pop();
                if (path.empty()) {
                    index = 0;
                    return;
                }
                // Если мы вернулись снизу из pointers[i], значит предыдущим ключом 
                // будет ключ с индексом i-1 в родителе.
                if (path.top().second > 0) {
                    index = path.top().second - 1;
                    return;
                }
                // Если в родителе мы тоже были на 0-м ключе, идем выше
            }
        }
    }

    template<typename tkey, typename NodePtrType, typename Comparator>
    size_t find_index_in_node(NodePtrType *node, const tkey &key, Comparator comp) {
        size_t left = 0;
        size_t right = node->_keys.size();
        while (left < right) {
            size_t mid = left + (right - left) / 2;
            if (comp(node->_keys[mid].first, key)) {
                left = mid + 1;
            } else {
                right = mid;
            }
        }

        return left;
    }

    template<typename tkey, typename NodePtrType, typename Comparator>
    size_t find_upper_index_in_node(NodePtrType* node, const tkey& key, Comparator comp) {
        size_t left = 0;
        size_t right = node->_keys.size();
        while (left < right) {
            size_t mid = left + (right - left) / 2;
            if (!comp(key, reinterpret_cast<const tkey&>(node->_keys[mid]))) {
                left = mid + 1;
            } else {
                right = mid;
            }
        }
        return left;
    }
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::btree_iterator::btree_iterator(
        const std::stack<std::pair<btree_node**, size_t>>& path, size_t index) : _path(path), _index(index)
{
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator::reference
B_tree<tkey, tvalue, compare, t>::btree_iterator::operator*() const noexcept
{
    using node_ptr_type = btree_node*;
    return btree_details::get_node_data<reference, node_ptr_type>(_path.top());
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator::pointer
B_tree<tkey, tvalue, compare, t>::btree_iterator::operator->() const noexcept
{
    return &(operator*());
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator&
B_tree<tkey, tvalue, compare, t>::btree_iterator::operator++()
{
    using const_stack_t = std::stack<std::pair<btree_node* const*, size_t>>;
    btree_details::increment_logic(reinterpret_cast<const_stack_t &>(_path), _index);
    return *this;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator
B_tree<tkey, tvalue, compare, t>::btree_iterator::operator++(int)
{
    btree_node temp = *this;
    ++(*this);
    return temp;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator&
B_tree<tkey, tvalue, compare, t>::btree_iterator::operator--()
{
    using const_stack_t = std::stack<std::pair<btree_node* const*, size_t>>;
    btree_details::decrement_logic(reinterpret_cast<const_stack_t&>(_path), _index);
    return *this;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator
B_tree<tkey, tvalue, compare, t>::btree_iterator::operator--(int)
{
    btree_node temp = *this;
    --(*this);
    return temp;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_iterator::operator==(const self& other) const noexcept
{
    return _path == other._path && _index == other._index;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_iterator::operator!=(const self& other) const noexcept
{
    return !(*this == other);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_iterator::depth() const noexcept
{
    return _path.empty() ? 0 : _path.size() - 1;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_iterator::current_node_keys_count() const noexcept
{
    if (_path.empty()) {
        return 0;
    }

    return (*_path.top().first)->_keys.size();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_iterator::is_terminate_node() const noexcept
{
    if (_path.empty()) {
        return false;
    }

    return (*_path.top().first)->_pointers.empty();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_iterator::index() const noexcept
{
    if (_path.empty()) {
        return 0;
    }

    return _index;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::btree_const_iterator::btree_const_iterator(
        const std::stack<std::pair<btree_node* const*, size_t>>& path, size_t index) : _path(path), _index(index)
{
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::btree_const_iterator::btree_const_iterator(
        const btree_iterator& it) noexcept
{
    auto temp_stack = it._path;
    _path = reinterpret_cast<const std::stack<std::pair<btree_node* const*, size_t>>&>(it._path);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator::reference
B_tree<tkey, tvalue, compare, t>::btree_const_iterator::operator*() const noexcept
{
    return btree_details::get_node_data<reference>(_path.top());
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator::pointer
B_tree<tkey, tvalue, compare, t>::btree_const_iterator::operator->() const noexcept
{
    return &(operator*());
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator&
B_tree<tkey, tvalue, compare, t>::btree_const_iterator::operator++()
{
    btree_details::increment_logic(_path, _index);
    return *this;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator
B_tree<tkey, tvalue, compare, t>::btree_const_iterator::operator++(int)
{
    btree_const_iterator temp = *this;
    ++(*this);
    return temp;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator&
B_tree<tkey, tvalue, compare, t>::btree_const_iterator::operator--()
{
    btree_details::decrement_logic(_path, _index);
    return *this;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator
B_tree<tkey, tvalue, compare, t>::btree_const_iterator::operator--(int)
{
    btree_const_iterator temp = *this;
    --(*this);
    return temp;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_const_iterator::operator==(const self& other) const noexcept
{
    return _path == other._path && _index == other._index;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_const_iterator::operator!=(const self& other) const noexcept
{
    return !(*this == other);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_const_iterator::depth() const noexcept
{
    return _path.empty() ? 0 : _path.size() - 1;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_const_iterator::current_node_keys_count() const noexcept
{
    if (_path.empty()) {
        return 0;
    }

    return (*_path.top().first)->_keys.size();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_const_iterator::is_terminate_node() const noexcept
{
    if (_path.empty()) {
        return false;
    }

    return (*_path.top().first)->_pointers.empty();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_const_iterator::index() const noexcept
{
    if (_path.empty()) {
        return 0;
    }
    
    return _index;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::btree_reverse_iterator(
        const std::stack<std::pair<btree_node**, size_t>>& path, size_t index) : _path(path), _index(index)
{
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::btree_reverse_iterator(
        const btree_iterator& it) noexcept
{
    _path = it._path;
    _index = it.index;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::operator B_tree<tkey, tvalue, compare, t>::btree_iterator() const noexcept
{
    return btree_iterator(_path, _index);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::reference
B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::operator*() const noexcept
{
    return btree_details::get_node_data<reference>(_path.top());
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::pointer
B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::operator->() const noexcept
{
    return &(operator*());
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator&
B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::operator++()
{
    using const_stack_t = std::stack<std::pair<btree_node* const*, size_t>>;
    btree_details::decrement_logic(reinterpret_cast<const_stack_t&>(_path), _index);
    return *this;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator
B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::operator++(int)
{
    btree_reverse_iterator temp = *this;
    ++(*this);
    return temp;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator&
B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::operator--()
{
    using const_stack_t = std::stack<std::pair<btree_node* const*, size_t>>;
    btree_details::increment_logic(reinterpret_cast<const_stack_t&>(_path), _index);
    return *this;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator
B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::operator--(int)
{
    btree_reverse_iterator temp = *this;
    --(*this);
    return temp;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::operator==(const self& other) const noexcept
{
    return _index == other._index && _path == other._path;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::operator!=(const self& other) const noexcept
{
    return !(*this == other);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::depth() const noexcept
{
    return _path.empty() ? 0 : _path.size() - 1;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::current_node_keys_count() const noexcept
{
    if (_path.empty()) {
        return 0;
    }

    return (*_path.top())->_keys.size();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::is_terminate_node() const noexcept
{
    if (_path.empty()) {
        return false;
    }

    btree_node *node = *_path.top().first;
    return node->_pointers.empty();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::index() const noexcept
{
    if (_path.empty()) {
        return 0;
    }
    
    return _index;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::btree_const_reverse_iterator(
        const std::stack<std::pair<btree_node* const*, size_t>>& path, size_t index) : _path(path), _index(index)
{
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::btree_const_reverse_iterator(
        const btree_reverse_iterator& it) noexcept
{
    _index = it._index;

    std::stack<std::pair<btree_node**, size_t>> temp_stack = it._path;
    std::vector<std::pair<btree_node* const*, size_t>> elements;
    while (!temp_stack.empty()) {
        std::pair<btree_node **, size_t> &top = temp_stack.top();
        elements.push_back({static_cast<btree_node* const*>(top.first), top.second});
        temp_stack.pop();
    }

    for (auto i = elements.rbegin(); i != elements.rend(); i++) {
        _path.push(*i);
    }
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::operator B_tree<tkey, tvalue, compare, t>::btree_const_iterator() const noexcept
{
    return btree_const_iterator(_path, _index);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::reference
B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::operator*() const noexcept
{
    return btree_details::get_node_data<reference>(_path.top());
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::pointer
B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::operator->() const noexcept
{
    return &(operator*());
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator&
B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::operator++()
{
    btree_details::decrement_logic(_path, _index);
    return *this;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator
B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::operator++(int)
{
    btree_const_reverse_iterator temp = *this;
    ++(*this);
    return temp;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator&
B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::operator--()
{
    btree_details::increment_logic(_path, _index);
    return *this;
} 

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator
B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::operator--(int)
{
    btree_const_reverse_iterator temp = *this;
    --(*this);
    return temp;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::operator==(const self& other) const noexcept
{
    return _path == other._path && _index == other._index;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::operator!=(const self& other) const noexcept
{
    return !(*this == other);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::depth() const noexcept
{
    return _path.empty() ? 0 : _path.size() - 1;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::current_node_keys_count() const noexcept
{
    if (_path.empty()) {
        return 0;
    }

    btree_node *node = *_path.top().first;
    return node->_keys.size();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::is_terminate_node() const noexcept
{
    if (_path.empty()) {
        return false;
    }

    btree_node *node = *_path.top().first;
    return node->_pointers.empty();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::index() const noexcept
{
    if (_path.empty()) {
        return 0;
    }
    
    return _index;
}

// endregion iterators implementation

// region element access implementation

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
tvalue& B_tree<tkey, tvalue, compare, t>::at(const tkey& key)
{
    btree_iterator it = find(key);
    if (it == end()) {
        throw std::out_of_range("B_tree::at: key not found");
    }

    return it->second;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
const tvalue& B_tree<tkey, tvalue, compare, t>::at(const tkey& key) const
{
    btree_iterator it = find(key);
    if (it == cend()) {
        throw std::out_of_range("B_tree::at: key not found (const)");
    }

    return it->second;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
tvalue& B_tree<tkey, tvalue, compare, t>::operator[](const tkey& key)
{
    btree_iterator it = find(key);
    if (it == end()) {
        auto res = insert(key, tvalue());
        return res.first->second;
    }

    return it->second;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
tvalue& B_tree<tkey, tvalue, compare, t>::operator[](tkey&& key)
{
    btree_iterator it = find(key);
    if (it == end()) {
        auto res = insert(std::move(key), tvalue());
        return res.first->second;
    }

    return it->second;
}

// endregion element access implementation

// region iterator begins implementation

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator B_tree<tkey, tvalue, compare, t>::begin()
{
    btree_iterator it;
    if (!_root) {
        return it;
    }

    btree_node **cur = &_root;
    while (*cur) {
        it._path.push({const_cast<btree_node **>(cur), 0});
        if ((*cur)->_pointers.empty()) {
            break;
        }

        cur = &((*cur)->_pointers[0]);
    }

    it._index = 0;
    return it;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator B_tree<tkey, tvalue, compare, t>::end()
{
    return btree_iterator();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator B_tree<tkey, tvalue, compare, t>::begin() const
{
    btree_const_iterator it;
    if (!_root) {
        return it;
    }

    btree_node* const* cur = &_root;
    while (*cur) {
        it._path.push({cur, 0});
        if ((*cur)->_pointers.empty()) {
            break;
        }

        cur = &((*cur)->_pointers[0]);
    }

    it._index = 0;
    return it;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator B_tree<tkey, tvalue, compare, t>::end() const
{
    return btree_const_iterator();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator B_tree<tkey, tvalue, compare, t>::cbegin() const
{
    return begin();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator B_tree<tkey, tvalue, compare, t>::cend() const
{
    return end();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator B_tree<tkey, tvalue, compare, t>::rbegin()
{
    btree_reverse_iterator it;
    if (!_root) {
        return it;
    }

    btree_node **cur = &_root;
    while (*cur) {
        size_t child_index = (*cur)->_pointers.empty() ? 0 : (*cur)->_pointers.size() - 1;
        it._path.push({reinterpret_cast<btree_node* const*>(cur), child_index});
        if ((*cur)->_pointers.empty()) {
            break;
        }

        cur = &((*cur)->_pointers[child_index]);
    }

    it._index = (*it._path.top().first)->_keys.size() - 1;
    return it;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator B_tree<tkey, tvalue, compare, t>::rend()
{
    return btree_reverse_iterator();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator B_tree<tkey, tvalue, compare, t>::rbegin() const
{
    btree_const_reverse_iterator it;
    if (!_root) {
        return it;
    } 

    btree_node* const* cur = &_root;
    while (*cur != nullptr) {
        size_t child_index = (*cur)->_pointers.empty() ? 0 : (*cur)->_pointers.size() - 1;
        it._path.push({reinterpret_cast<btree_node* const*>(cur), child_index});
        if ((*cur)->_pointers.empty()) {
            break;
        }

        cur = &((*cur)->_pointers[child_index]);
    }

    it._index = (*it._path.top().first)->_keys.size() - 1;
    return it;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator B_tree<tkey, tvalue, compare, t>::rend() const
{
    return btree_const_reverse_iterator();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator B_tree<tkey, tvalue, compare, t>::crbegin() const
{
    return rbegin();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator B_tree<tkey, tvalue, compare, t>::crend() const
{
    return rend();
}

// endregion iterator begins implementation

// region lookup implementation

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::size() const noexcept
{
    return _size;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::empty() const noexcept
{
    return !_root || _size == 0; 
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator B_tree<tkey, tvalue, compare, t>::find(const tkey& key)
{
    if (!_root) {
        return end();
    }

    btree_iterator it;
    btree_node **cur = &_root;
    while (*cur) {
        size_t i = btree_details::find_index_in_node(*cur, key, static_cast<const compare&>(*this));
        it._path.push({const_cast<btree_node **>(cur), i});
        if (i < (*cur)->_keys.size() && !static_cast<const compare&>(*this)(key, reinterpret_cast<const tkey &>((*cur)->_keys[i]))) {
            it._index = i;
            return it;
        }

        if ((*cur)->_pointers.empty()) {
            break;
        }

        cur = &((*cur)->_pointers[i]);
    }

    return end();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator B_tree<tkey, tvalue, compare, t>::find(const tkey& key) const
{
    btree_const_iterator it;
    if (!_root) {
        return end();
    }

    btree_node* const* cur = &_root;
    while (*cur) {
        size_t i = btree_details::find_index_in_node(*cur, key, static_cast<const compare&>(*this));
        it._path.push({reinterpret_cast<btree_node* const*>(cur), i});
        if (i < (*cur)->_keys.size() && !_comparator(key, reinterpret_cast<const tkey &>((*cur)->_keys[i]))) {
            it._index = i;
            return it;
        }

        if ((*cur)->_pointers.empty()) {
            break;
        }

        cur = &((*cur)->_pointers[i]);
    }

    return end();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator B_tree<tkey, tvalue, compare, t>::lower_bound(const tkey& key)
{
    if (!_root) {
        return end();
    }

    btree_iterator it;
    btree_node **cur = &_root;
    const compare &cmp = static_cast<const compare &>(*this);

    while (*cur != nullptr) {
        std::size_t i = btree_details::find_index_in_node(*cur, key, cmp);
        it._path.push({const_cast<btree_node **>(cur), i});

        if (i < (*cur)->_keys.size() && !cmp(key, reinterpret_cast<const tkey &>((*cur)->_keys[i]))) {
            it._index = i;
            return it;
        }

        if ((*cur)->_pointers.empty()) {
            break;
        }

        cur = &((*cur)->_pointers[i]);
    }

    return end();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator B_tree<tkey, tvalue, compare, t>::lower_bound(const tkey& key) const
{
    btree_const_iterator it;
    if (!_root) {
        return end();
    }

    btree_node* const* cur = &_root;
    btree_const_iterator last_ge = end();
    while (*cur) {
        size_t i = btree_details::find_index_in_node(*cur, key, static_cast<const compare&>(*this));
        it._path.push({reinterpret_cast<btree_node* const*>(cur), i});
        if (i < (*cur)->_keys.size() && !_comparator(key, reinterpret_cast<const tkey &>((*cur)->_keys[i]))) {
            it._index = i;
            return it;
        }

        if (i < (*cur)->_keys.size()) {
            last_ge = it;
            last_ge._index = i;
        }

        if ((*cur)->_pointers.empty()) {
            break;
        }

        cur = &((*cur)->_pointers[i]);
    }

    return last_ge;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator B_tree<tkey, tvalue, compare, t>::upper_bound(const tkey& key)
{
    btree_iterator it;
    if (!_root) {
        return end();
    }

    btree_node **cur = &_root;
    btree_iterator last_ge = end();
    while (*cur) {
        size_t i = btree_details::find_upper_index_in_node(*cur, key, static_cast<const compare&>(*this));
        it._path.push({reinterpret_cast<btree_node* const*>(cur), i});

        if (i < (*cur)->_keys.size()) {
            last_ge = it;
            last_ge._index = i;
        }

        if ((*cur)->_pointers.empty()) {
            break;
        }

        cur = &((*cur)->_pointers[i]);
    }

    return last_ge;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator B_tree<tkey, tvalue, compare, t>::upper_bound(const tkey& key) const
{
    btree_const_iterator it;
    if (!_root) {
        return end();
    }

    btree_node* const* cur = &_root;
    btree_const_iterator last_ge = end();
    while (*cur) {
        size_t i = btree_details::find_upper_index_in_node(*cur, key, static_cast<const compare&>(*this));
        it._path.push({reinterpret_cast<btree_node* const*>(cur), i});

        if (i < (*cur)->_keys.size()) {
            last_ge = it;
            last_ge._index = i;
        }

        if ((*cur)->_pointers.empty()) {
            break;
        }

        cur = &((*cur)->_pointers[i]);
    }

    return last_ge;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::contains(const tkey& key) const
{
    if (!_root) {
        return false;
    }

    btree_node* const* cur = &_root;
    while (*cur) {
        size_t i = btree_details::find_index_in_node(*cur, key, static_cast<const compare&>(*this));
        if (i < (*cur)->_keys.size() && !_comparator(key, reinterpret_cast<const tkey &>((*cur)->_keys[i]))) {
            return true;
        }

        if ((*cur)->_pointers.empty()) {
            break;
        }

        cur = &((*cur)->_pointers[i]);
    }

    return false;
}

// endregion lookup implementation

// region modifiers implementation
template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
void B_tree<tkey, tvalue, compare, t>::r_clear(btree_node *node) noexcept {
    if (!node) {
        return;
    }

    for (auto child : node->_pointers) {
        r_clear(child);
    }

    delete node;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
void B_tree<tkey, tvalue, compare, t>::clear() noexcept
{
    r_clear(_root);
    _root = nullptr;
    _size = 0;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
void B_tree<tkey, tvalue, compare, t>::split_child(btree_node *parent, std::size_t index) {
    btree_node *y = parent->_pointers[index];
    btree_node *z = new btree_node();

    const std::size_t split_index = t;

    for (std::size_t i = split_index + 1; i < y->_keys.size(); i++) {
        z->_keys.push_back(std::move(y->_keys[i]));
    }


    if (!y->_pointers.empty()) {
        for (std::size_t i = split_index + 1; i < y->_pointers.size(); i++) {
            z->_pointers.push_back(y->_pointers[i]);
        }

        y->_pointers.resize(split_index + 1);
    }

    parent->_keys.insert(parent->_keys.begin() + index, std::move(y->_keys[split_index]));
    parent->_pointers.insert(parent->_pointers.begin() + index + 1, z);

    y->_keys.resize(split_index);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
void B_tree<tkey, tvalue, compare, t>::insert_non_full(btree_node *node, tree_data_type &&data) {
    const auto &cmp = static_cast<const compare &>(*this);
    size_t i = btree_details::find_index_in_node(node, data.first, cmp);

    if (node->_pointers.empty()) {
        node->_keys.insert(node->_keys.begin() + i, std::move(data));
    } else {
        insert_non_full(node->_pointers[i], std::move(data));
        
        if (node->_pointers[i]->_keys.size() == 2 * t) {
            split_child(node, i);
        }
    }
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
std::pair<typename B_tree<tkey, tvalue, compare, t>::btree_iterator, bool>
B_tree<tkey, tvalue, compare, t>::insert(const tree_data_type& data)
{
    tree_data_type copy = data;
    return insert(std::move(copy));
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
std::pair<typename B_tree<tkey, tvalue, compare, t>::btree_iterator, bool>
B_tree<tkey, tvalue, compare, t>::insert(tree_data_type&& data)
{
    auto it = find(data.first);
    if (it != end()) {
        return {it, false};
    }

    if (!_root) {
        _root = new btree_node();
        _root->_keys.push_back(std::move(data));
        _size = 1;
        return {find(reinterpret_cast<const tkey &>(_root->_keys[0])), true};
    }

    const tkey &key_to_find = data.first;
    insert_non_full(_root, std::move(data));

    if (_root->_keys.size() == 2 * t) {
        btree_node *new_root = new btree_node();
        new_root->_pointers.push_back(_root);
        split_child(new_root, 0);
        _root = new_root;
    }
    _size++;

    return {find(key_to_find), true};
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
template<typename... Args>
std::pair<typename B_tree<tkey, tvalue, compare, t>::btree_iterator, bool>
B_tree<tkey, tvalue, compare, t>::emplace(Args&&... args)
{
    tree_data_type data(std::forward<Args>(args)...);
    return insert(std::move(data));
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator
B_tree<tkey, tvalue, compare, t>::insert_or_assign(const tree_data_type& data)
{
    auto it = find(data.first);
    if (it != end()) {
        it->second = data.second;
        return it;
    }

    return insert(data).first;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator
B_tree<tkey, tvalue, compare, t>::insert_or_assign(tree_data_type&& data)
{
    auto it = find(data.first);
    if (it != end()) {
        it->second = std::move(data.second);
        return it;
    }

    return insert(std::move(data)).first;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
template<typename... Args>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator
B_tree<tkey, tvalue, compare, t>::emplace_or_assign(Args&&... args)
{
    tree_data_type data(std::forward<Args>(args)...);

    auto it = find(data.first);
    if (it != end()) {
        it->second = std::move(data.second);
        return it;
    }

    return insert(std::move(data)).first;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
void B_tree<tkey, tvalue, compare, t>::fill_child(btree_node *parent, std::size_t idx) {
    if (idx != 0 && parent->_pointers[idx - 1]->_keys.size() >= t) {
        borrow_from_prev(parent, idx);
    } else if (idx != parent->_keys.size() && parent->_pointers[idx + 1]->_keys.size() >= t) {
        borrow_from_next(parent, idx);
    } else {
        if (idx != parent->_keys.size()) {
            merge_nodes(parent, idx);
        } else {
            merge_nodes(parent, idx - 1);
        }
    }
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
void B_tree<tkey, tvalue, compare, t>::borrow_from_prev(btree_node *parent, std::size_t idx) {
    btree_node *child = parent->_pointers[idx];
    btree_node *sibling = parent->_pointers[idx - 1];

    child->_keys.insert(child->_keys.begin(), std::move(parent->_keys[idx - 1]));
    if (!child->_pointers.empty()) {
        child->_pointers.insert(child->_pointers.begin(), sibling->_pointers.back());
        sibling->_pointers.pop_back();
    }

    parent->_keys[idx - 1] = std::move(sibling->_keys.back());
    sibling->_keys.pop_back();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
void B_tree<tkey, tvalue, compare, t>::borrow_from_next(btree_node *parent, std::size_t idx) {
    btree_node *child = parent->_pointers[idx];
    btree_node *sibling = parent->_pointers[idx + 1];

    child->_keys.push_back(std::move(parent->_keys[idx]));
    if (!child->_pointers.empty()) {
        child->_pointers.push_back(sibling->_pointers.front());
        sibling->_pointers.erase(sibling->_pointers.begin());
    }

    parent->_keys[idx] = std::move(sibling->_keys.front());
    sibling->_keys.erase(sibling->_keys.begin());
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
void B_tree<tkey, tvalue, compare, t>::merge_nodes(btree_node *parent, std::size_t idx) {
    btree_node *left = parent->_pointers[idx];
    btree_node *right = parent->_pointers[idx + 1];

    left->_keys.push_back(std::move(parent->_keys[idx]));

    for (auto& k : right->_keys) {
        left->_keys.push_back(std::move(k));
    }

    if (!left->_pointers.empty()) {
        for (auto& p : right->_pointers) {
            left->_pointers.push_back(p);
        }
    }

    parent->_keys.erase(parent->_keys.begin() + idx);
    parent->_pointers.erase(parent->_pointers.begin() + idx + 1);
    delete right;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
void B_tree<tkey, tvalue, compare, t>::handle_internal_node_delete(btree_node *node, std::size_t idx) {
    const tkey &k = reinterpret_cast<const tkey &>(node->_keys[idx].first);
    
    if (node->_pointers[idx]->_keys.size() >= t) {
        tree_data_type pred = get_predecessor(node->_pointers[idx]);
        node->_keys[idx] = std::move(pred);
        remove_recursive(node->_pointers[idx], reinterpret_cast<const tkey&>(node->_keys[idx].first));
    } else if (node->_pointers[idx + 1]->_keys.size() >= t) {
        tree_data_type succ = get_successor(node->_pointers[idx + 1]);
        node->_keys[idx] = std::move(succ);
        remove_recursive(node->_pointers[idx + 1], reinterpret_cast<const tkey&>(node->_keys[idx].first));
    } else {
        merge_nodes(node, idx);
        remove_recursive(node->_pointers[idx], k);
    }
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::tree_data_type
B_tree<tkey, tvalue, compare, t>::get_predecessor(btree_node *node) {
    while (!node->_pointers.empty()) {
        node = node->_pointers.back();
    }

    return node->_keys.back();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::tree_data_type
B_tree<tkey, tvalue, compare, t>::get_successor(btree_node *node) {
    while (!node->_pointers.empty()) {
        node = node->_pointers.front();
    }

    return node->_keys.front();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
void B_tree<tkey, tvalue, compare, t>::remove_recursive(btree_node *node, const tkey &key)
{
    auto &cmp = static_cast<const compare &>(*this);
    std::size_t idx = btree_details::find_index_in_node(node, key, cmp);

    if (idx < node->_keys.size() && !cmp(key, reinterpret_cast<const tkey &>(node->_keys[idx].first))) {
        if (node->_pointers.empty()) {
            node->_keys.erase(node->_keys.begin() + idx);
        } else {
            handle_internal_node_delete(node, idx);
        }
    } else {
        if (node->_pointers.empty()) {
            return;
        }

        if (node->_pointers[idx]->_keys.size() < t) {
            fill_child(node, idx);
        }

        if (idx > node->_keys.size()) {
            remove_recursive(node->_pointers[idx - 1], key);
        } else {
            remove_recursive(node->_pointers[idx], key);
        }
    }
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator
B_tree<tkey, tvalue, compare, t>::erase(btree_iterator pos)
{
    if (pos == end()) {
        return end();
    }

    return erase(pos->first);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator
B_tree<tkey, tvalue, compare, t>::erase(btree_const_iterator pos)
{
    if (pos == cend()) {
        return end();
    }

    return erase(pos->first);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator
B_tree<tkey, tvalue, compare, t>::erase(btree_iterator beg, btree_iterator en)
{
    while (beg != en) {
        beg = erase(beg);
    }

    return en;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator
B_tree<tkey, tvalue, compare, t>::erase(btree_const_iterator beg, btree_const_iterator en)
{
    if (beg == en) {
        return (en == cend()) ? end() : lower_bound(en->first);
    }

    bool until_end = (en == cend());
    tkey end_key;
    if (!until_end) {
        end_key = en->first;
    }

    while (beg != cend() && (until_end || static_cast<const compare &>(*this)(beg->first, end_key))) {
        beg = erase(beg);
    }

    return lower_bound(until_end ? tkey() : end_key);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator
B_tree<tkey, tvalue, compare, t>::erase(const tkey& key)
{
    if (!_root) {
        return end();
    }

    remove_recursive(_root, key);

    if (_root->_keys.empty()) {
        btree_node *old_root = _root;
        if (!_root->_pointers.empty()) {
            _root = _root->_pointers[0];
        } else {
            _root = nullptr;
        }

        delete old_root;
    }

    _size--;
    return lower_bound(key);
}

// endregion modifiers implementation

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool compare_pairs(const typename B_tree<tkey, tvalue, compare, t>::tree_data_type &lhs,
                   const typename B_tree<tkey, tvalue, compare, t>::tree_data_type &rhs)
{
    return compare_keys<tkey, tvalue, compare, t>(lhs.first, rhs.first);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool compare_keys(const tkey &lhs, const tkey &rhs)
{
    return compare{}(lhs, rhs);
}

#endif