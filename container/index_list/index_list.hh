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
    static constexpr size_t npos = static_cast<size_t>(-1);

    // -----------------------------------------------------------------
    //  Node
    // -----------------------------------------------------------------
    struct Node {
        T      value;
        size_t prev;
        size_t next;

        Node(T v, size_t p, size_t n)
            : value(std::move(v)), prev(p), next(n) {}
    };

private:
    std::vector<Node> nodes_;
    std::vector<size_t> free_list_;
    size_t head_ = npos;
    size_t tail_ = npos;
    size_t size_ = 0;

    // -----------------------------------------------------------------
    //  Allocation
    // -----------------------------------------------------------------
    size_t alloc_node(T v, size_t prev, size_t next)
    {
        size_t idx;
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

    void free_node(size_t idx)
    {
        free_list_.push_back(idx);
    }

    void link(size_t prev, size_t next)
    {
        if (prev != npos) nodes_[prev].next = next;
        if (next != npos) nodes_[next].prev = prev;
    }

public:
    // -----------------------------------------------------------------
    //  Construction
    // -----------------------------------------------------------------
    explicit IndexList(size_t capacity = 64)
    {
        nodes_.reserve(capacity);
        free_list_.reserve(capacity);
    }

    // -----------------------------------------------------------------
    //  Push / Emplace
    // -----------------------------------------------------------------
    void push_back(T v)
    {
        size_t idx = alloc_node(std::move(v), tail_, npos);
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
        size_t idx = alloc_node(std::move(v), npos, head_);
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
        size_t idx = alloc_node(T(std::forward<Args>(args)...), tail_, npos);
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
        size_t idx = alloc_node(T(std::forward<Args>(args)...), npos, head_);
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
        size_t old = tail_;
        tail_ = nodes_[old].prev;
        if (tail_ != npos) nodes_[tail_].next = npos;
        else head_ = npos;
        free_node(old);
        --size_;
    }

    void pop_front()
    {
        assert(!empty() && "pop_front on empty list");
        size_t old = head_;
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
    void erase(size_t idx)
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
        size_t curr = head_;
        while (curr != npos) {
            size_t next = nodes_[curr].next;
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
    [[nodiscard]] size_t size() const noexcept { return size_; }

    [[nodiscard]] size_t front_index() const noexcept { return head_; }
    [[nodiscard]] size_t back_index() const noexcept { return tail_; }

    [[nodiscard]] std::optional<size_t> next_index(size_t idx) const
    {
        if (idx == npos || idx >= nodes_.size()) return std::nullopt;
        size_t n = nodes_[idx].next;
        return n == npos ? std::nullopt : std::make_optional(n);
    }

    // -----------------------------------------------------------------
    //  Access by index
    // -----------------------------------------------------------------
    T& operator[](size_t idx)
    {
        assert(idx < nodes_.size());
        return nodes_[idx].value;
    }

    const T& operator[](size_t idx) const
    {
        assert(idx < nodes_.size());
        return nodes_[idx].value;
    }
};
