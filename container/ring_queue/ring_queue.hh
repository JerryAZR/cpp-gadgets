// container/ring_queue/ring_queue.hh
#pragma once

#include <vector>
#include <cassert>
#include <utility>
#include <cstddef>
#include <type_traits>

/**
 * @file   ring_queue.hh
 * @brief  Dynamic ring queue backed by std::vector.
 *
 *  * Power-of-two capacity → wrap-around is a cheap `& (cap-1)`.
 *  * Automatic doubling growth.
 *  * Strong exception guarantee on push/emplace.
 *  * C++17 (gem5 compatible).
 */
template<class T>
class RingQueue {
  public:
    //==========================================================================//
    //  Construction
    //==========================================================================//

    /**
     * @brief Constructs a ring queue with a given *initial* capacity.
     *
     * The supplied capacity is **rounded up** to the next power-of-two so that
     * wrap-around can be performed with a cheap bitwise-AND (`& (capacity()-1)`).
     *
     * @param init_cap  Desired minimum capacity (default = 16). Must be greater than 0.
     *
     * @post `capacity()` is a power-of-two and `size() == 0`.
     */
    explicit RingQueue(std::size_t init_cap = 16)
        : data_(reserve_power_of_two(init_cap))
        , cap_mask_(data_.size() - 1)
    {
        assert(init_cap > 0 && "initial capacity must be >0");
    }

    //==========================================================================//
    //  Core API
    //==========================================================================//

    /**
     * @brief Pushes a value to the back of the queue.
     *
     * If the queue is full the internal buffer is grown (doubling strategy)
     * before the element is placed.
     *
     * @tparam U  Type of the argument (deduced).
     * @param  val R-value or l-value to be moved/copied into the queue.
     *
     * @pre `!full()` after a possible grow.
     * @post `size()` is increased by 1.
     *
     * @note Provides the strong exception guarantee: if construction of `T`
     *       throws, the queue is left unchanged.
     */
    template<class U>
    void push(U&& val)
    {
        ensure_capacity();
        new (data_.data() + tail_) T(std::forward<U>(val));
        advance_tail();
    }

    /**
     * @brief Constructs an object **in-place** at the back of the queue.
     *
     * The buffer is grown automatically if needed.
     *
     * @tparam Args  Parameter-pack for the constructor of `T`.
     * @param  args  Arguments forwarded to `T`'s constructor.
     *
     * @return Reference to the newly constructed element.
     *
     * @pre `!full()` after a possible grow.
     * @post `size()` is increased by 1.
     *
     * @note Strong exception guarantee – the queue remains unchanged if the
     *       constructor throws.
     */
    template<class... Args>
    T& emplace(Args&&... args)
    {
        ensure_capacity();
        new (data_.data() + tail_) T(std::forward<Args>(args)...);
        T& r = data_[tail_];
        advance_tail();
        return r;
    }

    /**
     * @brief Accesses the front element (mutable).
     *
     * @return Reference to the oldest element.
     *
     * @pre `!empty()`.
     */
    T& front()
    {
        assert(!empty() && "front() on empty queue");
        return data_[head_];
    }

    /**
     * @brief Accesses the front element (read-only).
     *
     * @return Const reference to the oldest element.
     *
     * @pre `!empty()`.
     */
    const T& front() const
    {
        assert(!empty() && "front() on empty queue");
        return data_[head_];
    }

    /**
     * @brief Accesses the back element (mutable).
     *
     * @return Reference to the youngest element.
     *
     * @pre `!empty()`.
     */
    T& back()
    {
        assert(!empty() && "back() on empty queue");
        std::size_t last = (tail_ - 1) & cap_mask_;
        return data_[last];
    }

    /**
     * @brief Accesses the back element (read-only).
     *
     * @return Const reference to the youngest element.
     *
     * @pre `!empty()`.
     */
    const T& back() const
    {
        assert(!empty() && "back() on empty queue");
        std::size_t last = (tail_ - 1) & cap_mask_;
        return data_[last];
    }

    /**
     * @brief Removes the front element.
     *
     * The destructor of the removed object is called explicitly.
     *
     * @pre `!empty()`.
     * @post `size()` is decreased by 1.
     */
    void pop()
    {
        assert(!empty() && "pop() on empty queue");
        data_[head_].~T();
        advance_head();
    }

    //==========================================================================//
    //  Queries
    //==========================================================================//

    /**
     * @brief Checks whether the queue contains no elements.
     *
     * @return `true` if `size() == 0`.
     */
    [[nodiscard]] bool empty() const noexcept { return size() == 0; }

    /**
     * @brief Checks whether the queue is completely full.
     *
     * @return `true` if `size() == capacity()`.
     */
    [[nodiscard]] bool full() const noexcept { return size() == capacity(); }

    /**
     * @brief Returns the number of elements currently stored.
     *
     * @return Current logical size of the queue.
     */
    [[nodiscard]] std::size_t size() const noexcept { return count_; }

    /**
     * @brief Returns the current storage capacity.
     *
     * The value is always a power-of-two.
     *
     * @return Number of slots available before a grow is required.
     */
    [[nodiscard]] std::size_t capacity() const noexcept { return data_.size(); }

    /**
     * @brief Ensures that at least *n* slots are available.
     *
     * If `n` exceeds the current capacity the buffer is grown to the
     * smallest power-of-two that is greater than or equal to `n`.
     *
     * @param n  Minimum capacity requested.
     *
     * @post `capacity() >= n`.
     */
    void reserve(std::size_t n)
    {
        if (n > capacity()) grow_to(next_power_of_two(n));
    }

    /**
     * @brief Reduces memory usage when the queue is sparsely populated.
     *
     * * If the queue is empty – the underlying vector is cleared.
     * * Otherwise, if `size() < capacity()/4` and `capacity() > 16`,
     *   the buffer is shrunk to `max(size()*2, 16)` (still a power-of-two).
     *
     * The operation is optional and rarely needed in tight loops.
     */
    void shrink_to_fit()
    {
        if (count_ == 0) {
            data_.clear();
            data_.shrink_to_fit();
            reset_indices();
        } else if (count_ < capacity() / 4 && capacity() > 16) {
            grow_to(std::max(next_power_of_two(count_), std::size_t(16)));
        }
    }

private:
    // --------------------------------------------------------------------- //
    //                     Internal growth logic
    // --------------------------------------------------------------------- //

    std::vector<T> data_;          ///< Contiguous storage
    std::size_t head_ = 0;           ///< Index of oldest element
    std::size_t tail_ = 0;           ///< Index where next push goes
    std::size_t count_ = 0;          ///< Live element count
    std::size_t cap_mask_ = 0;       ///< capacity()-1, used for fast wrap

    // ----------------------------------------------------------------- //
    //  Ensure room for one more element.
    // ----------------------------------------------------------------- //
    void ensure_capacity()
    {
        if (full()) grow();
    }

    // ----------------------------------------------------------------- //
    //  Double the current capacity.
    // ----------------------------------------------------------------- //
    void grow()
    {
        const std::size_t new_cap = capacity() ? capacity() * 2 : 1;
        grow_to(new_cap);
    }

    // ----------------------------------------------------------------- //
    //  Grow to *exactly* new_cap (must be power-of-two).
    // ----------------------------------------------------------------- //
    void grow_to(std::size_t new_cap)
    {
        static_assert(std::is_unsigned_v<std::size_t>, "std::size_t must be unsigned");

        std::vector<T> new_data(new_cap);               // uninitialized
        const std::size_t new_mask = new_cap - 1;

        // Copy in logical order, using fast wrap (head_ + i) & old_mask
        for (std::size_t i = 0; i < count_; ++i) {
            const std::size_t src = (head_ + i) & cap_mask_;
            new (&new_data[i]) T(std::move_if_noexcept(data_[src]));
            data_[src].~T();
        }

        data_    = std::move(new_data);
        head_    = 0;
        tail_    = count_;
        cap_mask_ = new_mask;
    }

    // ----------------------------------------------------------------- //
    //  Fast index wrap-around using bit-and.
    // ----------------------------------------------------------------- //
    void advance_head()
    {
        head_ = (head_ + 1) & cap_mask_;
        --count_;
    }
    void advance_tail()
    {
        tail_ = (tail_ + 1) & cap_mask_;
        ++count_;
    }

    // ----------------------------------------------------------------- //
    //  Power-of-two helpers (compile-time safe)
    // ----------------------------------------------------------------- //
    static constexpr bool is_power_of_two(std::size_t n) noexcept
    {
        return n > 0 && (n & (n - 1)) == 0;
    }

    static std::size_t next_power_of_two(std::size_t n) noexcept
    {
        if (n == 0) return 1;

    #if defined(__GNUC__) || defined(__clang__)
        return std::size_t(1) << (64 - __builtin_clzll(n - 1));
    #else
        // Portable fallback (slower, but correct)
        std::size_t p = 1;
        while (p < n) p <<= 1;
        return p;
    #endif
    }

    static std::size_t reserve_power_of_two(std::size_t n)
    {
        const std::size_t cap = next_power_of_two(n);
        assert(is_power_of_two(cap));
        return cap;
    }

    void reset_indices()
    {
        head_ = tail_ = count_ = 0;
        cap_mask_ = data_.size() - 1;
    }
};