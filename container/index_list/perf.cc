// container/index_list/perf.cc
// Performance benchmark: IndexList<long> vs std::list<long>

#include "index_list.hh"
#include <list>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <random>
#include <vector>
#include <cassert>

using Clock = std::chrono::high_resolution_clock;
using us    = std::chrono::microseconds;

constexpr size_t N = 10'000'000;
constexpr int RUNS = 5;

template<class Func>
double bench(Func&& f, int runs = RUNS)
{
    double best = 1e9;
    for (int i = 0; i < runs; ++i) {
        auto start = Clock::now();
        f();
        auto end = Clock::now();
        double t = std::chrono::duration_cast<us>(end - start).count();
        if (t < best) best = t;
    }
    return best;
}

int main()
{
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "=== IndexList<long> vs std::list<long> | N = " << N << " | runs = " << RUNS << " ===\n\n";

    // -----------------------------------------------------------------
    // 1. push_back N elements
    // -----------------------------------------------------------------
    {
        auto il = [] { IndexList<long> l; for (size_t i = 0; i < N; ++i) l.push_back(i); };
        auto sl = [] { std::list<long> l;  for (size_t i = 0; i < N; ++i) l.push_back(i); };
        double t1 = bench(il);
        double t2 = bench(sl);
        std::cout << "1. push_back × " << N << "\n"
                  << "   IndexList : " << t1 << " µs\n"
                  << "   std::list : " << t2 << " µs\n"
                  << "   Speedup   : " << t2/t1 << "×\n\n";
    }

    // -----------------------------------------------------------------
    // 2. pop_back N elements
    // -----------------------------------------------------------------
    {
        IndexList<long> il_pre; std::list<long> sl_pre;
        for (size_t i = 0; i < N; ++i) { il_pre.push_back(i); sl_pre.push_back(i); }

        auto il = [&] { auto l = il_pre; while (!l.empty()) l.pop_back(); };
        auto sl = [&] { auto l = sl_pre; while (!l.empty()) l.pop_back(); };
        double t1 = bench(il);
        double t2 = bench(sl);
        std::cout << "2. pop_back × " << N << "\n"
                  << "   IndexList : " << t1 << " µs\n"
                  << "   std::list : " << t2 << " µs\n"
                  << "   Speedup   : " << t2/t1 << "×\n\n";
    }

    // -----------------------------------------------------------------
    // 3. push_front N elements
    // -----------------------------------------------------------------
    {
        auto il = [] { IndexList<long> l; for (size_t i = 0; i < N; ++i) l.push_front(i); };
        auto sl = [] { std::list<long> l;  for (size_t i = 0; i < N; ++i) l.push_front(i); };
        double t1 = bench(il);
        double t2 = bench(sl);
        std::cout << "3. push_front × " << N << "\n"
                  << "   IndexList : " << t1 << " µs\n"
                  << "   std::list : " << t2 << " µs\n"
                  << "   Speedup   : " << t2/t1 << "×\n\n";
    }

    // -----------------------------------------------------------------
    // 4. pop_front N elements
    // -----------------------------------------------------------------
    {
        IndexList<long> il_pre; std::list<long> sl_pre;
        for (size_t i = 0; i < N; ++i) { il_pre.push_back(i); sl_pre.push_back(i); }

        auto il = [&] { auto l = il_pre; while (!l.empty()) l.pop_front(); };
        auto sl = [&] { auto l = sl_pre; while (!l.empty()) l.pop_front(); };
        double t1 = bench(il);
        double t2 = bench(sl);
        std::cout << "4. pop_front × " << N << "\n"
                  << "   IndexList : " << t1 << " µs\n"
                  << "   std::list : " << t2 << " µs\n"
                  << "   Speedup   : " << t2/t1 << "×\n\n";
    }

    // -----------------------------------------------------------------
    // 5. 1 push_back + 1 pop_back (N pairs)
    // -----------------------------------------------------------------
    {
        auto il = [] {
            IndexList<long> l;
            for (size_t i = 0; i < N; ++i) {
                l.push_back(i);
                l.pop_back();
            }
        };
        auto sl = [] {
            std::list<long> l;
            for (size_t i = 0; i < N; ++i) {
                l.push_back(i);
                l.pop_back();
            }
        };
        double t1 = bench(il);
        double t2 = bench(sl);
        std::cout << "5. 1 push_back + 1 pop_back × " << N << "\n"
                  << "   IndexList : " << t1 << " µs\n"
                  << "   std::list : " << t2 << " µs\n"
                  << "   Speedup   : " << t2/t1 << "×\n\n";
    }

    // -----------------------------------------------------------------
    // 6. 8 push_back + 8 pop_back (N/8 batches)
    // -----------------------------------------------------------------
    {
        constexpr size_t B = 8;
        auto il = [] {
            IndexList<long> l;
            for (size_t i = 0; i < N; i += B) {
                for (size_t j = 0; j < B; ++j) l.push_back(i + j);
                for (size_t j = 0; j < B; ++j) l.pop_back();
            }
        };
        auto sl = [] {
            std::list<long> l;
            for (size_t i = 0; i < N; i += B) {
                for (size_t j = 0; j < B; ++j) l.push_back(i + j);
                for (size_t j = 0; j < B; ++j) l.pop_back();
            }
        };
        double t1 = bench(il);
        double t2 = bench(sl);
        std::cout << "6. 8 push_back + 8 pop_back × " << N/B << "\n"
                  << "   IndexList : " << t1 << " µs\n"
                  << "   std::list : " << t2 << " µs\n"
                  << "   Speedup   : " << t2/t1 << "×\n\n";
    }

    // -----------------------------------------------------------------
    // 7. Random mix (push/pop front/back + remove_if)
    // -----------------------------------------------------------------
    {
        std::mt19937 rng(42);
        std::uniform_int_distribution<int> op(0, 5);
        std::uniform_int_distribution<long> val(0, 99);

        auto il = [&] {
            IndexList<long> l;
            for (size_t i = 0; i < N; ++i) {
                switch (op(rng)) {
                    case 0: l.push_back(val(rng)); break;
                    case 1: l.push_front(val(rng)); break;
                    case 2: if (!l.empty()) l.pop_back(); break;
                    case 3: if (!l.empty()) l.pop_front(); break;
                    case 4: if (!l.empty() && rng()%4==0) l.remove_if([](long x){ return x%7==0; }); break;
                }
            }
        };
        auto sl = [&] {
            std::list<long> l;
            std::mt19937 r(42);
            std::uniform_int_distribution<int> o(0, 5);
            std::uniform_int_distribution<long> v(0, 99);
            for (size_t i = 0; i < N; ++i) {
                switch (o(r)) {
                    case 0: l.push_back(v(r)); break;
                    case 1: l.push_front(v(r)); break;
                    case 2: if (!l.empty()) l.pop_back(); break;
                    case 3: if (!l.empty()) l.pop_front(); break;
                    case 4: if (!l.empty() && r()%4==0) l.remove_if([](long x){ return x%7==0; }); break;
                }
            }
        };
        double t1 = bench(il);
        double t2 = bench(sl);
        std::cout << "7. Random mix (" << N << " ops)\n"
                  << "   IndexList : " << t1 << " µs\n"
                  << "   std::list : " << t2 << " µs\n"
                  << "   Speedup   : " << t2/t1 << "×\n\n";
    }

    // -----------------------------------------------------------------
    // 8. remove_if (50% removal) — pure middle-removal test
    // -----------------------------------------------------------------
    {
        std::cout << "8. remove_if (50% removal)\n";
        IndexList<long> il_full; std::list<long> sl_full;
        for (size_t i = 0; i < N; ++i) {
            il_full.push_back(i);
            sl_full.push_back(i);
        }

        auto il = [&] {
            auto l = il_full;
            l.remove_if([](long x) { return x % 2 == 0; });  // remove even
        };
        auto sl = [&] {
            auto l = sl_full;
            l.remove_if([](long x) { return x % 2 == 0; });
        };

        double t1 = bench(il);
        double t2 = bench(sl);
        std::cout << "   IndexList : " << t1 << " µs\n"
                  << "   std::list : " << t2 << " µs\n"
                  << "   Speedup   : " << t2/t1 << "×\n\n";
    }

    std::cout << "All benchmarks complete.\n";
    return 0;
}
