// container/bitvec/test.cc
// Comprehensive correctness test for BitVec<N> (bitset-backed)

#include "bit_vector.hh"
#include <array>
#include <bitset>
#include <random>
#include <iostream>
#include <iomanip>
#include <cassert>
#include <cstddef>

// ====================================================================
//  7-Scenario Test (templated on RNG)
// ====================================================================
template<std::size_t N, class RNG>
void comprehensive_test_once(RNG& rng)
{
    std::uniform_int_distribution<std::size_t> pos(0, N-1);
    std::uniform_int_distribution<int> bit(0, 1);

    // 1. Randomize golden bitset
    std::bitset<N> golden;
    for (std::size_t i = 0; i < N; ++i) golden[i] = bit(rng);

    // 2. Create BitVec from golden
    BitVec<N> bv(golden);

    // Basic sanity
    assert((std::bitset<N>(bv) == golden) && "initial copy failed");

    // 3. Random valid range [lo, hi] (lo <= hi)
    std::size_t lo = pos(rng);
    std::size_t hi = pos(rng);
    if (lo > hi) std::swap(lo, hi);
    const std::size_t width = hi - lo + 1;

    // 4. Range read → compare with naive extraction
    std::bitset<N> extracted_naive;
    for (std::size_t i = 0; i < width; ++i) extracted_naive[i] = golden[lo + i];

    std::bitset<N> extracted_bv = bv(hi, lo);
    assert(extracted_bv == extracted_naive && "range read failed");

    // 5. Single-bit access within range
    if (width > 0) {
        std::size_t local = pos(rng) % width;
        std::size_t global = lo + local;
        bool bv_bit = bv(hi, lo)[local];
        bool golden_bit = golden[global];
        assert(bv_bit == golden_bit && "single-bit access failed");
    }

    // 6. Toggle a random bit in range
    if (width > 0) {
        std::size_t local = pos(rng) % width;
        std::size_t global = lo + local;
        bv(hi, lo)[local].flip();
        golden.flip(global);
        assert(bv(hi, lo)[local] == golden[global] && "bit toggle failed");
    }

    // 7. Assignment tests
    // 7a: assign from uint64_t (only if width <= 64)
    if (width <= 64) {
        uint64_t val = rng();
        uint64_t mask = width == 64 ? ~uint64_t(0) : (uint64_t(1) << width) - 1;
        val &= mask;

        bv(hi, lo) = val;

        for (std::size_t i = 0; i < width; ++i) {
            golden[lo + i] = (val >> i) & 1;
        }

        assert((std::bitset<N>(bv) == golden) && "uint64_t assignment failed");
    }

    // 7b: assign from another slice
    if (width > 0) {
        std::size_t lo2 = pos(rng);
        std::size_t hi2 = lo2 + width - 1;
        if (hi2 >= N) hi2 = N - 1;
        std::size_t src_width = hi2 - lo2 + 1;

        // Store old state for debugging
        std::bitset<N> old_bv = std::bitset<N>(bv);
        std::bitset<N> old_golden = golden;

        // Perform assignment
        bv(hi, lo) = bv(hi2, lo2);

        // Apply same to golden model (with proper width)
        for (std::size_t i = 0; i < hi - lo + 1; ++i) {
            golden[lo + i] = i < src_width ? old_golden[lo2 + i] : 0;
        }

        std::bitset<N> new_bv = std::bitset<N>(bv);

        if (new_bv != golden) {
            std::cerr << "\n=== SLICE ASSIGNMENT FAILED ===\n"
                    << "Range: (" << hi << "," << lo << ")  width=" << width << "\n"
                    << "Source range: (" << hi2 << "," << lo2 << ")  src_width=" << src_width << "\n"
                    << "Old BitVec : " << old_bv << "\n"
                    << "Old Golden : " << old_golden << "\n"
                    << "New BitVec : " << new_bv << "\n"
                    << "New Golden : " << golden << "\n"
                    << "=====================================\n";
            std::abort();
        }
    }
}

// ====================================================================
//  Multi-iteration runner
// ====================================================================
template<std::size_t N, std::size_t Iterations = 10000>
void stress_test_bitvec(std::mt19937::result_type seed = 42)
{
    std::mt19937 rng(seed);

    std::cout << "=== BitVec<" << N << "> stress test | "
              << Iterations << " iterations | seed = " << seed << " ===\n";

    for (std::size_t i = 0; i < Iterations; ++i) {
        comprehensive_test_once<N>(rng);
        if ((i + 1) % (Iterations / 4) == 0) {
            std::cout << "  " << (i + 1) << " / " << Iterations << " passed\n";
        }
    }

    std::cout << "All " << Iterations << " iterations passed for N=" << N << "\n\n";
}

template<std::size_t... Ns>
void run_all()
{
    (stress_test_bitvec<Ns>(), ...);   // C++17 fold expression
}

// ====================================================================
//  Main
// ====================================================================
int main()
{
    run_all<63,64,65,127,128,129,199,255>();

    std::cout << "All BitVec tests passed!\n";
    return 0;
}
