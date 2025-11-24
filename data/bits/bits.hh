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

template <size_t N, size_t W>
class BitSlice {
public:
    static_assert(W > 0, "BitSlice width must be > 0");
    static_assert(W <= N, "BitSlice width cannot exceed parent bitset size");

    BitSlice(std::bitset<N>& data, size_t lo)
        : data(data), lo(lo)
    {}

    size_t size() const { return W; }

    // --- Read ---
    bool operator[](size_t idx) const {
        return data[lo + idx];
    }

    // --- Write ---
    typename std::bitset<N>::reference operator[](size_t idx) {
        return data[lo + idx];
    }

    void set(size_t idx, bool value) {
        data[lo + idx] = value;
    }

    // --- Assignment from any BitVec-like type ---
    template <typename OtherT>
    BitSlice& operator=(const OtherT& rhs) {
        // Convert rhs → std::bitset<N>
        std::bitset<N> rhs_bits = make_bitset<N>(rhs) << lo;

        // Prepare mask at the correct offset
        std::bitset<N> mask = make_bitmask<N>(W) << lo;

        data = (data & ~mask) | (rhs_bits & mask);
        return *this;
    }

    // Copy assignment from same-width slice
    BitSlice& operator=(const BitSlice& rhs) {
        return this->operator=<BitSlice>(rhs);
    }

    // --- Conversion to std::bitset<M> ---
    template <size_t M>
    explicit operator std::bitset<M>() const {
        std::bitset<M> result = make_bitset<M>(data >> lo);
        result &= make_bitmask<M>(W);
        return result;
    }

    operator std::bitset<W>() const {
        std::bitset<W> result = make_bitset<W>(data >> lo);
        return result;
    }

private:
    std::bitset<N>& data;
    size_t lo;
};

template <size_t N, size_t M, size_t W>
std::bitset<N> make_bitset(const BitSlice<M, W>& slice)
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
class bits {
public:
    using size_type = std::size_t;

    // -----------------------------------------------------------------
    //  Constructors / Assignment
    // -----------------------------------------------------------------
    bits() = default;
    bits(const std::bitset<N>& bs) : data(bs) {}
    bits(std::bitset<N>&& bs) noexcept : data(std::move(bs)) {}

    bits& operator=(const std::bitset<N>& bs) { data = bs; return *this; }
    bits& operator=(std::bitset<N>&& bs) noexcept { data = std::move(bs); return *this; }

    // Defaulted special members
    bits(const bits&) = default;
    bits(bits&&) noexcept = default;
    bits& operator=(const bits&) = default;
    bits& operator=(bits&&) noexcept = default;

    // Compare BitVec<N> with BitVec<N>
    bool operator==(const bits<N>& other) const noexcept {
        return data == other.data;
    }

    bool operator!=(const bits<N>& other) const noexcept {
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

    std::string to_string() const {
        return data.to_string();
    }

    // -----------------------------------------------------------------
    //  Optional: expose common bitset methods directly (for discoverability)
    // -----------------------------------------------------------------
    bool test(size_type pos) const { return data.test(pos); }
    bits& set(size_type pos, bool val = true) { data.set(pos, val); return *this; }
    bits& reset(size_type pos) { data.reset(pos); return *this; }
    bits& flip(size_type pos) { data.flip(pos); return *this; }

    bool any() const noexcept { return data.any(); }
    bool all() const noexcept { return data.all(); }
    bool none() const noexcept { return data.none(); }
    size_type count() const noexcept { return data.count(); }

    // ------------------------------
    // Slice: full version <HI, LO>
    // ------------------------------
    template <size_t HI, size_t LO>
    auto slice() {
        constexpr size_t W = HI - LO + 1;
        static_assert(HI < N && LO < N && HI >= LO);
        return BitSlice<N, W>(data, LO);
    }

    template <size_t HI, size_t LO>
    auto slice() const {
        constexpr size_t W = HI - LO + 1;
        static_assert(HI < N && LO < N && HI >= LO);
        return BitSlice<N, W>(data, LO);
    }

    // --------------------------------
    // slice_lo<LO, W>
    // --------------------------------
    template <size_t W>
    auto slice_lo(size_t lo) {
        return BitSlice<N, W>(data, lo);
    }

    template <size_t W>
    auto slice_lo(size_t lo) const {
        return BitSlice<N, W>(data, lo);
    }

    // --------------------------------
    // slice_hi<HI, W>
    // --------------------------------
    template <size_t W>
    auto slice_hi(size_t hi) {
        return BitSlice<N, W>(data, hi + 1 - W);
    }

    template <size_t W>
    auto slice_hi(size_t hi) const {
        return BitSlice<N, W>(data, hi + 1 - W);
    }

private:
    std::bitset<N> data{};
};

template <size_t N>
std::ostream& operator<<(std::ostream& os, const bits<N>& bv) {
    return os << static_cast<const std::bitset<N>&>(bv);
}
