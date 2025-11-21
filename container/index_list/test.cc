#include "index_list.hh"

#include <list>
#include <random>
#include <iostream>
#include <iomanip>
#include <cassert>
#include <memory>
#include <optional>
#include <utility>
#include <cstdlib>

// ====================================================================
//  Checker
// ====================================================================
template<class T>
void check_index_list(const IndexList<T>& il, const std::list<T>& golden, std::size_t iter) {
    if (il.size() != golden.size()) {
        std::cerr << "ITER " << iter << " SIZE FAIL: " << il.size() << " vs " << golden.size() << "\n";
        std::abort();
    }
    if (il.empty() != golden.empty()) {
        std::cerr << "ITER " << iter << " EMPTY FAIL\n";
        std::abort();
    }
    if (!golden.empty()) {
        if (il.front() != golden.front()) {
            std::cerr << "ITER " << iter << " FRONT FAIL\n";
            std::abort();
        }
        if (il.back() != golden.back()) {
            std::cerr << "ITER " << iter << " BACK FAIL\n";
            std::abort();
        }
    }

    auto it = golden.begin();
    auto idx = il.front_index();
    while (it != golden.end() && idx != IndexList<T>::npos) {
        if (il[idx] != *it) {
            std::cerr << "ITER " << iter << " TRAVERSAL FAIL\n";
            std::abort();
        }
        ++it;
        auto opt = il.next_index(idx);
        idx = opt ? *opt : IndexList<T>::npos;
    }
    if (it != golden.end() || idx != IndexList<T>::npos) {
        std::cerr << "ITER " << iter << " LENGTH FAIL\n";
        std::abort();
    }
}

// ====================================================================
//  Sync wrappers
// ====================================================================
template<class T, class U> void sync_push_back(IndexList<T>& il, std::list<T>& l, U&& v) { il.push_back(std::forward<U>(v)); l.push_back(std::forward<U>(v)); }
template<class T, class U> void sync_push_front(IndexList<T>& il, std::list<T>& l, U&& v) { il.push_front(std::forward<U>(v)); l.push_front(std::forward<U>(v)); }
template<class T, class... A> void sync_emplace_back(IndexList<T>& il, std::list<T>& l, A&&... a) { il.emplace_back(std::forward<A>(a)...); l.emplace_back(std::forward<A>(a)...); }
template<class T, class... A> void sync_emplace_front(IndexList<T>& il, std::list<T>& l, A&&... a) { il.emplace_front(std::forward<A>(a)...); l.emplace_front(std::forward<A>(a)...); }
template<class T> void sync_pop_back(IndexList<T>& il, std::list<T>& l) { assert(!il.empty()); il.pop_back(); l.pop_back(); }
template<class T> void sync_pop_front(IndexList<T>& il, std::list<T>& l) { assert(!il.empty()); il.pop_front(); l.pop_front(); }
template<class T, class P> void sync_remove_if(IndexList<T>& il, std::list<T>& l, P p) { il.remove_if(p); l.remove_if(p); }

// ====================================================================
//  Stress test loop
// ====================================================================
template<class T, std::size_t Iters = 200'000>
void stress_test(std::mt19937::result_type seed) {
    IndexList<T> il;
    std::list<T> golden;
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> op(0, 6), val(0, 99);

    std::cout << "=== IndexList Test | " << typeid(T).name() << " | Seed: " << seed << " ===\n";

    for (std::size_t i = 0; i < Iters; ++i) {
        switch (op(rng)) {
            case 0: sync_push_back(il, golden, T(val(rng))); break;
            case 1: sync_push_front(il, golden, T(val(rng))); break;
            case 2: sync_emplace_back(il, golden, val(rng)); break;
            case 3: sync_emplace_front(il, golden, val(rng)); break;
            case 4: if (!il.empty()) sync_pop_back(il, golden); break;
            case 5: if (!il.empty()) sync_pop_front(il, golden); break;
            case 6: if (!il.empty() && rng()%2) {
                auto pred = [&](const T& x) { return x % 7 == 0; };
                sync_remove_if(il, golden, pred);
            } break;
        }
        check_index_list(il, golden, i);
    }
    std::cout << "PASSED " << Iters << " ops\n\n";
}

// ====================================================================
//  Main
// ====================================================================
std::mt19937::result_type get_seed(int argc, char** argv) {
    if (argc >= 2) {
        try { return std::stoull(argv[1]); }
        catch (...) { std::cerr << "Bad seed, using random\n"; }
    }
    return std::random_device{}();
}

int main(int argc, char** argv) {
    auto seed = get_seed(argc, argv);

    // Test 1: long
    stress_test<long>(seed);

    // Test 2: std::shared_ptr<long>
    // stress_test<std::shared_ptr<long>>(seed);

    std::cout << "All IndexList tests passed!\n";
    return 0;
}
