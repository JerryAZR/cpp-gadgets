// bitvec_bitset.hh
#pragma once

#include <bitset>
#include <cstddef>
#include <type_traits>

/**
 * @brief Create a std::bitset<N> mask with the lowest 'w' bits set to 1.
 *
 * Hot path (w < 64):  direct uint64_t mask
 * Cold path (w >= 64): doubling algorithm
 *
 * @tparam N  Total bit width of the bitset
 * @param w   Number of low bits to set (0 <= w <= N)
 * @return    std::bitset<N> with lowest w bits = 1, rest = 0
 */
template<std::size_t N>
[[nodiscard]] constexpr std::bitset<N> make_bitmask(std::size_t w)
{
    if (w < 64) [[likely]] {
        // Fast path: w < 64 -> direct uint64_t mask
        const uint64_t low_mask = (uint64_t(1) << w) - 1;
        return std::bitset<N>(low_mask);
    }

    // Cold path: w >= 64 → doubling algorithm
    std::bitset<N> mask;
    std::size_t n = 64;
    mask = std::bitset<N>(~uint64_t(0));  // fill first 64 bits

    while (n < w) {
        std::size_t shift = std::min(n, w - n);
        mask |= (mask << shift);
        n <<= 1;
    }

    return mask;
}


template <size_t N>
std::bitset<N> make_bitset(unsigned long long src)
{
    return std::bitset<N>(src);
}


template <size_t N, size_t M>
std::bitset<N> make_bitset(const std::bitset<M>& src)
{
    std::bitset<N> dst;

    using ullong = unsigned long long;

    // --- Fast-path cases ---
    if constexpr (M == N) {
        dst = src;
        return dst;
    }

    if constexpr (M <= 64) {
        // Safe: lower 64 bits contain all information
        ullong v = src.to_ullong();
        dst = std::bitset<N>(v);     // automatic zero-extension if N > 64
        return dst;                  // automatic truncation if N < 64
    }

    // --- General case: copy in 64-bit chunks ---
    constexpr size_t WORD_BITS  = sizeof(ullong) * 8;
    constexpr size_t SRC_WORDS  = (M + WORD_BITS - 1) / WORD_BITS;
    constexpr size_t DST_WORDS  = (N + WORD_BITS - 1) / WORD_BITS;

    size_t words_to_copy = std::min(SRC_WORDS, DST_WORDS);

    for (size_t w = 0; w < words_to_copy; ++w) {
        // Extract low 64 bits of (src >> (64*w))
        ullong chunk = (src >> (w * WORD_BITS)).to_ullong();

        size_t bitpos = w * WORD_BITS;
        std::bitset<N> tmp(chunk);
        tmp <<= bitpos;
        dst |= tmp;
    }

    return dst;
}


template <size_t N>
class BitSlice {
public:
    BitSlice(std::bitset<N>& data, size_t hi, size_t lo)
        : data(data), lo(lo), width(hi - lo + 1) {}

    size_t size() const { return width; }

    // read
    bool operator[](size_t idx) const {
        return data[lo + idx];
    }

    // write
    typename std::bitset<N>::reference operator[](size_t idx) {
        return data[lo + idx];
    }
    void set(size_t idx, bool value) {
        data[lo + idx] = value;
    }

    // assignment from another BitVec-like type
    template <typename OtherT>
    BitSlice& operator=(const OtherT& rhs) {
        std::bitset<N> bits = make_bitset<N>(rhs) << lo;
        std::bitset<N> mask = make_bitmask<N>(width) << lo;
        data = (data & ~mask) | (bits & mask);
        return *this;
    }

    // Explicit copy assignment — forwards to templated version
    BitSlice& operator=(const BitSlice& rhs) {
        return this->operator=<BitSlice>(rhs);
    }

    // explicit conversion to a fixed BitVec<M>
    template <size_t M>
    explicit operator std::bitset<M>() const {
        std::bitset<M> result = make_bitset<M>(data >> lo);
        result &= make_bitmask<M>(width);
        return result;
    }

    operator std::bitset<N>() const {
        std::bitset<N> result = data >> lo;
        result &= make_bitmask<N>(width);
        return result;
    }

private:
    std::bitset<N>& data;
    size_t lo;
    size_t width;
};

template <size_t N, size_t M>
std::bitset<N> make_bitset(const BitSlice<M>& slice)
{
    return (std::bitset<N>)slice;
}


/**
 * @brief Thin, zero-overhead wrapper around std::bitset<N>
 *
 * Purpose:
 * - Provide a base for future extensions (slice, concat, SystemVerilog-like syntax)
 * - Allow implicit use of all std::bitset methods
 * - Serve as a drop-in replacement with better naming / future features
 *
 * Current cost: **zero** compared to raw std::bitset<N>
 */
template <std::size_t N>
class BitVec {
public:
    using size_type = std::size_t;

    // -----------------------------------------------------------------
    //  Constructors / Assignment
    // -----------------------------------------------------------------
    BitVec() = default;
    BitVec(const std::bitset<N>& bs) : data(bs) {}
    BitVec(std::bitset<N>&& bs) noexcept : data(std::move(bs)) {}

    BitVec& operator=(const std::bitset<N>& bs) { data = bs; return *this; }
    BitVec& operator=(std::bitset<N>&& bs) noexcept { data = std::move(bs); return *this; }

    // Defaulted special members
    BitVec(const BitVec&) = default;
    BitVec(BitVec&&) noexcept = default;
    BitVec& operator=(const BitVec&) = default;
    BitVec& operator=(BitVec&&) noexcept = default;

    // Compare BitVec<N> with BitVec<N>
    bool operator==(const BitVec<N>& other) const noexcept {
        return data == other.data;
    }

    bool operator!=(const BitVec<N>& other) const noexcept {
        return data != other.data;
    }

    // Compare BitVec<N> with std::bitset<N>
    bool operator==(const std::bitset<N>& other) const noexcept {
        return data == other;
    }

    bool operator!=(const std::bitset<N>& other) const noexcept {
        return data != other;
    }

    // -----------------------------------------------------------------
    //  Implicit conversion to underlying bitset
    // -----------------------------------------------------------------
    operator const std::bitset<N>&() const noexcept { return data; }
    operator std::bitset<N>&() noexcept { return data; }

    // -----------------------------------------------------------------
    //  Optional: expose common bitset methods directly (for discoverability)
    // -----------------------------------------------------------------
    bool test(size_type pos) const { return data.test(pos); }
    BitVec& set(size_type pos, bool val = true) { data.set(pos, val); return *this; }
    BitVec& reset(size_type pos) { data.reset(pos); return *this; }
    BitVec& flip(size_type pos) { data.flip(pos); return *this; }

    bool any() const noexcept { return data.any(); }
    bool all() const noexcept { return data.all(); }
    bool none() const noexcept { return data.none(); }
    size_type count() const noexcept { return data.count(); }

    // --- Slice API ---
    BitSlice<N> operator()(size_t hi, size_t lo) {
        return BitSlice<N>(data, hi, lo);
    }

    const BitSlice<N> operator()(size_t hi, size_t lo) const {
        return BitSlice<N>(data, hi, lo);
    }

private:
    std::bitset<N> data{};
};
