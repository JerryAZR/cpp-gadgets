// container/ring_queue/perf.cc
#include "ring_queue.hh"
#include <deque>
#include <chrono>
#include <random>
#include <vector>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <cassert>

using Clock = std::chrono::high_resolution_clock;
using ns = std::chrono::nanoseconds;

// ---------------------------------------------------------------------
//  Benchmark runner
// ---------------------------------------------------------------------
struct Result {
    double mean_ns;
    double stddev_ns;
    double ops_per_sec;
};

template<class Func>
Result benchmark(Func&& f, int runs = 8)
{
    std::vector<double> times;
    times.reserve(runs);

    f(); // Warmup;

    for (int r = 0; r < runs; ++r) {
        auto start = Clock::now();
        f();
        auto end = Clock::now();
        times.push_back(std::chrono::duration_cast<ns>(end - start).count());
    }

    double sum = 0, sum_sq = 0;
    for (double t : times) {
        sum += t;
        sum_sq += t * t;
    }
    double mean = sum / runs;
    double variance = (sum_sq / runs) - (mean * mean);
    double stddev = std::sqrt(variance);

    return { mean, stddev, 1e9 / mean };
}

// ---------------------------------------------------------------------
//  Test element
// ---------------------------------------------------------------------
using Element = long;  // Change to Packet for move-only test

// ---------------------------------------------------------------------
//  Main
// ---------------------------------------------------------------------
int main()
{
    constexpr size_t N = 100'000'000;
    constexpr int SEED = 42;

    std::cout << "=== RingQueue vs std::deque Performance ===\n"
              << "Element: " << typeid(Element).name() << "\n"
              << "N = " << N << "\n\n";

    // -----------------------------------------------------------------
    //  Scenario 1: Add N elements
    // -----------------------------------------------------------------
    {
        std::cout << "1. Add " << N << " elements\n";
        auto rq_add = [&]() {
            RingQueue<Element> q;
            for (size_t i = 0; i < N; ++i) q.push(i);
        };
        auto dq_add = [&]() {
            std::deque<Element> q;
            for (size_t i = 0; i < N; ++i) q.push_back(i);
        };

        auto r1 = benchmark(rq_add);
        auto d1 = benchmark(dq_add);
        std::cout << "   RingQueue: " << std::fixed << std::setprecision(2)
                  << r1.mean_ns / 1e6 << " ms (±" << r1.stddev_ns / 1e6 << " ms)\n"
                  << "   std::deque: " << d1.mean_ns / 1e6 << " ms (±" << d1.stddev_ns / 1e6 << " ms)\n"
                  << "   Speedup: " << std::setprecision(2) << d1.mean_ns / r1.mean_ns << "×\n\n";
    }

    // -----------------------------------------------------------------
    //  Scenario 2: Pop all N elements
    // -----------------------------------------------------------------
    {
        std::cout << "2. Pop " << N << " elements\n";
        RingQueue<Element> rq_full;
        std::deque<Element> dq_full;
        for (size_t i = 0; i < N; ++i) {
            rq_full.push(i);
            dq_full.push_back(i);
        }

        auto rq_pop = [&]() {
            RingQueue<Element> q = rq_full;
            while (!q.empty()) q.pop();
        };
        auto dq_pop = [&]() {
            std::deque<Element> q = dq_full;
            while (!q.empty()) q.pop_front();
        };

        auto r2 = benchmark(rq_pop);
        auto d2 = benchmark(dq_pop);
        std::cout << "   RingQueue: " << r2.mean_ns / 1e6 << " ms\n"
                  << "   std::deque: " << d2.mean_ns / 1e6 << " ms\n"
                  << "   Speedup: " << d2.mean_ns / r2.mean_ns << "×\n\n";
    }

    // -----------------------------------------------------------------
    //  Scenario 3: 1 add + 1 pop per iter
    // -----------------------------------------------------------------
    {
        std::cout << "3. 1 add + 1 pop per iteration (" << N << " pairs)\n";
        auto rq_pair = [&]() {
            RingQueue<Element> q;
            for (size_t i = 0; i < N; ++i) {
                q.push(i);
                q.pop();
            }
        };
        auto dq_pair = [&]() {
            std::deque<Element> q;
            for (size_t i = 0; i < N; ++i) {
                q.push_back(i);
                q.pop_front();
            }
        };

        auto r3 = benchmark(rq_pair);
        auto d3 = benchmark(dq_pair);
        std::cout << "   RingQueue: " << r3.mean_ns / 1e6 << " ms\n"
                  << "   std::deque: " << d3.mean_ns / 1e6 << " ms\n"
                  << "   Speedup: " << d3.mean_ns / r3.mean_ns << "×\n\n";
    }

    // -----------------------------------------------------------------
    //  Scenario 4: 8 add + 8 pop per iter
    // -----------------------------------------------------------------
    {
        std::cout << "4. 8 add + 8 pop per iteration (" << N/8 << " batches)\n";
        auto rq_batch = [&]() {
            RingQueue<Element> q;
            for (size_t i = 0; i < N; i += 8) {
                for (int j = 0; j < 8; ++j) q.push(i + j);
                for (int j = 0; j < 8; ++j) q.pop();
            }
        };
        auto dq_batch = [&]() {
            std::deque<Element> q;
            for (size_t i = 0; i < N; i += 8) {
                for (int j = 0; j < 8; ++j) q.push_back(i + j);
                for (int j = 0; j < 8; ++j) q.pop_front();
            }
        };

        auto r4 = benchmark(rq_batch);
        auto d4 = benchmark(dq_batch);
        std::cout << "   RingQueue: " << r4.mean_ns / 1e6 << " ms\n"
                  << "   std::deque: " << d4.mean_ns / 1e6 << " ms\n"
                  << "   Speedup: " << d4.mean_ns / r4.mean_ns << "×\n\n";
    }

    // -----------------------------------------------------------------
    //  Scenario 5: Random add/pop (same seed)
    // -----------------------------------------------------------------
    {
        std::cout << "5. Random add/pop (same seed, " << N << " ops)\n";
        std::mt19937 rng(SEED);
        std::uniform_int_distribution<int> op_dist(0, 1);  // 0=push, 1=pop

        auto rq_rand = [&]() {
            RingQueue<Element> q;
            std::mt19937 r(SEED);
            for (size_t i = 0; i < N; ++i) {
                if (op_dist(r) == 0 || q.empty()) {
                    q.push(i);
                } else {
                    q.pop();
                }
            }
        };
        auto dq_rand = [&]() {
            std::deque<Element> q;
            std::mt19937 r(SEED);
            for (size_t i = 0; i < N; ++i) {
                if (op_dist(r) == 0 || q.empty()) {
                    q.push_back(i);
                } else {
                    q.pop_front();
                }
            }
        };

        auto r5 = benchmark(rq_rand);
        auto d5 = benchmark(dq_rand);
        std::cout << "   RingQueue: " << r5.mean_ns / 1e6 << " ms\n"
                  << "   std::deque: " << d5.mean_ns / 1e6 << " ms\n"
                  << "   Speedup: " << d5.mean_ns / r5.mean_ns << "×\n\n";
    }

    std::cout << "All benchmarks complete.\n";
    return 0;
}
