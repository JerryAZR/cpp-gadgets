// container/index_list/index_list.hh
#pragma once

#include <vector>
#include <cstddef>
#include <cassert>
#include <optional>
#include <utility>
#include <functional>

/**
 * @file   index_list.hh
 * @brief  Index-linked list in a vector – O(1) middle removal, cache-friendly.
 *
 * Features:
 * - push/pop/emplace front/back
 * - erase(index), remove_if
 * - front()/back() accessors
 * - No pointers, no heap → gem5-safe
 */
template<class T>
class IndexList {
public:
    using value_type = T;
    using size_type  = std::size_t;
    using index_type = size_type;

    static constexpr index_type npos = static_cast<index_type>(-1);

    // -----------------------------------------------------------------
    //  Node
    // -----------------------------------------------------------------
    struct Node {
        T         value;
        index_type prev;
        index_type next;

        Node(T v, index_type p, index_type n)
            : value(std::move(v)), prev(p), next(n) {}
    };

private:
    std::vector<Node> nodes_;
    std::vector<index_type> free_list_;
    index_type head_ = npos;
    index_type tail_ = npos;
    size_type  size_ = 0;

    // -----------------------------------------------------------------
    //  Allocation
    // -----------------------------------------------------------------
    index_type alloc_node(T v, index_type prev, index_type next)
    {
        index_type idx;
        if (!free_list_.empty()) {
            idx = free_list_.back();
            free_list_.pop_back();
            nodes_[idx] = Node(std::move(v), prev, next);
        } else {
            idx = nodes_.size();
            nodes_.emplace_back(std::move(v), prev, next);
        }
        return idx;
    }

    void free_node(index_type idx)
    {
        free_list_.push_back(idx);
    }

    void link(index_type prev, index_type next)
    {
        if (prev != npos) nodes_[prev].next = next;
        if (next != npos) nodes_[next].prev = prev;
    }

public:
    // -----------------------------------------------------------------
    //  Construction
    // -----------------------------------------------------------------
    explicit IndexList(size_type capacity = 64)
    {
        nodes_.reserve(capacity);
        free_list_.reserve(capacity);
    }

    // -----------------------------------------------------------------
    //  Push / Emplace
    // -----------------------------------------------------------------
    void push_back(T v)
    {
        index_type idx = alloc_node(std::move(v), tail_, npos);
        if (empty()) {
            head_ = tail_ = idx;
        } else {
            link(tail_, idx);
            tail_ = idx;
        }
        ++size_;
    }

    void push_front(T v)
    {
        index_type idx = alloc_node(std::move(v), npos, head_);
        if (empty()) {
            head_ = tail_ = idx;
        } else {
            link(idx, head_);
            head_ = idx;
        }
        ++size_;
    }

    template<class... Args>
    T& emplace_back(Args&&... args)
    {
        index_type idx = alloc_node(T(std::forward<Args>(args)...), tail_, npos);
        if (empty()) {
            head_ = tail_ = idx;
        } else {
            link(tail_, idx);
            tail_ = idx;
        }
        ++size_;
        return nodes_[idx].value;
    }

    template<class... Args>
    T& emplace_front(Args&&... args)
    {
        index_type idx = alloc_node(T(std::forward<Args>(args)...), npos, head_);
        if (empty()) {
            head_ = tail_ = idx;
        } else {
            link(idx, head_);
            head_ = idx;
        }
        ++size_;
        return nodes_[idx].value;
    }

    // -----------------------------------------------------------------
    //  Pop
    // -----------------------------------------------------------------
    void pop_back()
    {
        assert(!empty() && "pop_back on empty list");
        index_type old = tail_;
        tail_ = nodes_[old].prev;
        if (tail_ != npos) nodes_[tail_].next = npos;
        else head_ = npos;
        free_node(old);
        --size_;
    }

    void pop_front()
    {
        assert(!empty() && "pop_front on empty list");
        index_type old = head_;
        head_ = nodes_[old].next;
        if (head_ != npos) nodes_[head_].prev = npos;
        else tail_ = npos;
        free_node(old);
        --size_;
    }

    // -----------------------------------------------------------------
    //  Accessors
    // -----------------------------------------------------------------
    T& front()
    {
        assert(!empty() && "front() on empty list");
        return nodes_[head_].value;
    }

    const T& front() const
    {
        assert(!empty() && "front() on empty list");
        return nodes_[head_].value;
    }

    T& back()
    {
        assert(!empty() && "back() on empty list");
        return nodes_[tail_].value;
    }

    const T& back() const
    {
        assert(!empty() && "back() on empty list");
        return nodes_[tail_].value;
    }

    // -----------------------------------------------------------------
    //  Erase / Remove
    // -----------------------------------------------------------------
    void erase(index_type idx)
    {
        assert(idx < nodes_.size() && "invalid index");
        auto& node = nodes_[idx];
        link(node.prev, node.next);
        if (head_ == idx) head_ = node.next;
        if (tail_ == idx) tail_ = node.prev;
        free_node(idx);
        --size_;
    }

    template<class Predicate>
    void remove_if(Predicate pred)
    {
        index_type curr = head_;
        while (curr != npos) {
            index_type next = nodes_[curr].next;
            if (pred(nodes_[curr].value)) {
                erase(curr);
            }
            curr = next;
        }
    }

    // -----------------------------------------------------------------
    //  Queries
    // -----------------------------------------------------------------
    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }
    [[nodiscard]] size_type size() const noexcept { return size_; }

    [[nodiscard]] index_type front_index() const noexcept { return head_; }
    [[nodiscard]] index_type back_index() const noexcept { return tail_; }

    [[nodiscard]] std::optional<index_type> next_index(index_type idx) const
    {
        if (idx == npos || idx >= nodes_.size()) return std::nullopt;
        index_type n = nodes_[idx].next;
        return n == npos ? std::nullopt : std::make_optional(n);
    }

    // -----------------------------------------------------------------
    //  Access by index
    // -----------------------------------------------------------------
    T& operator[](index_type idx)
    {
        assert(idx < nodes_.size());
        return nodes_[idx].value;
    }

    const T& operator[](index_type idx) const
    {
        assert(idx < nodes_.size());
        return nodes_[idx].value;
    }
};
