// test.cc – Complete IndexList correctness test (single file)
// Compile: g++ -std=c++17 -O3 -Wall -Wextra -march=native -I. -o test.run test.cc

#include <vector>
#include <list>
#include <random>
#include <iostream>
#include <iomanip>
#include <optional>
#include <utility>
#include <cassert>
#include <cstdlib>
#include <cstdint>

// ====================================================================
//  Packet (move-only, self-incrementing ID)
// ====================================================================
struct Packet {
    std::int64_t id;
    std::int64_t payload;
private:
    static inline std::int64_t next_id = 0;
public:
    Packet() noexcept : id(next_id++), payload(0) {}
    template<class Int> explicit Packet(Int p) noexcept : id(next_id++), payload(p) {}
    Packet(const Packet&) = delete;
    Packet& operator=(const Packet&) = delete;
    Packet(Packet&&) noexcept = default;
    Packet& operator=(Packet&&) noexcept = default;
    bool operator==(const Packet& o) const noexcept { return id == o.id && payload == o.payload; }
    // -----------------------------------------------------------------
    //  operator% – modulo on payload
    // -----------------------------------------------------------------
    long long operator%(long long mod) const noexcept {
        return payload % mod;
    }
};
inline std::ostream& operator<<(std::ostream& os, const Packet& p) {
    return os << "Packet{id=" << p.id << ", payload=" << p.payload << '}';
}

// ====================================================================
//  IndexList
// ====================================================================
template<class T>
class IndexList {
public:
    using size_type  = std::size_t;
    using index_type = size_type;
    static constexpr index_type npos = static_cast<index_type>(-1);

    struct Node {
        T value;
        index_type prev, next;
        Node(T v, index_type p, index_type n) : value(std::move(v)), prev(p), next(n) {}
    };

private:
    std::vector<Node> nodes_;
    std::vector<index_type> free_list_;
    index_type head_ = npos, tail_ = npos;
    size_type  size_ = 0;

    index_type alloc_node(T v, index_type prev, index_type next) {
        index_type idx;
        if (!free_list_.empty()) {
            idx = free_list_.back(); free_list_.pop_back();
            nodes_[idx] = Node(std::move(v), prev, next);
        } else {
            idx = nodes_.size();
            nodes_.emplace_back(std::move(v), prev, next);
        }
        return idx;
    }
    void free_node(index_type idx) { free_list_.push_back(idx); }
    void link(index_type p, index_type n) {
        if (p != npos) nodes_[p].next = n;
        if (n != npos) nodes_[n].prev = p;
    }

public:
    explicit IndexList(size_type cap = 64) { nodes_.reserve(cap); free_list_.reserve(cap); }

    void push_back(T v) {
        index_type idx = alloc_node(std::move(v), tail_, npos);
        if (empty()) head_ = tail_ = idx;
        else { link(tail_, idx); tail_ = idx; }
        ++size_;
    }
    void push_front(T v) {
        index_type idx = alloc_node(std::move(v), npos, head_);
        if (empty()) head_ = tail_ = idx;
        else { link(idx, head_); head_ = idx; }
        ++size_;
    }
    template<class... Args> T& emplace_back(Args&&... args) {
        index_type idx = alloc_node(T(std::forward<Args>(args)...), tail_, npos);
        if (empty()) head_ = tail_ = idx;
        else { link(tail_, idx); tail_ = idx; }
        ++size_;
        return nodes_[idx].value;
    }
    template<class... Args> T& emplace_front(Args&&... args) {
        index_type idx = alloc_node(T(std::forward<Args>(args)...), npos, head_);
        if (empty()) head_ = tail_ = idx;
        else { link(idx, head_); head_ = idx; }
        ++size_;
        return nodes_[idx].value;
    }

    void pop_back()  { assert(!empty()); index_type o = tail_; tail_ = nodes_[o].prev; if (tail_ != npos) nodes_[tail_].next = npos; else head_ = npos; free_node(o); --size_; }
    void pop_front() { assert(!empty()); index_type o = head_; head_ = nodes_[o].next; if (head_ != npos) nodes_[head_].prev = npos; else tail_ = npos; free_node(o); --size_; }

    T& front() { assert(!empty()); return nodes_[head_].value; }
    const T& front() const { assert(!empty()); return nodes_[head_].value; }
    T& back()  { assert(!empty()); return nodes_[tail_].value; }
    const T& back() const  { assert(!empty()); return nodes_[tail_].value; }

    void erase(index_type idx) {
        assert(idx < nodes_.size());
        auto& n = nodes_[idx];
        link(n.prev, n.next);
        if (head_ == idx) head_ = n.next;
        if (tail_ == idx) tail_ = n.prev;
        free_node(idx); --size_;
    }

    template<class Pred> void remove_if(Pred p) {
        index_type cur = head_;
        while (cur != npos) {
            index_type nxt = nodes_[cur].next;
            if (p(nodes_[cur].value)) erase(cur);
            cur = nxt;
        }
    }

    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }
    [[nodiscard]] size_type size() const noexcept { return size_; }
    [[nodiscard]] index_type front_index() const noexcept { return head_; }
    [[nodiscard]] index_type back_index() const noexcept { return tail_; }
    [[nodiscard]] std::optional<index_type> next_index(index_type i) const {
        return (i == npos || i >= nodes_.size()) ? std::nullopt : std::make_optional(nodes_[i].next == npos ? npos : nodes_[i].next);
    }

    T& operator[](index_type i) { return nodes_[i].value; }
    const T& operator[](index_type i) const { return nodes_[i].value; }
};

// ====================================================================
//  Checker
// ====================================================================
template<class T>
void check_index_list(const IndexList<T>& il, const std::list<T>& golden, std::size_t iter) {
    if (il.size() != golden.size()) { std::cerr << "SIZE FAIL at " << iter << "\n"; std::abort(); }
    if (il.empty() != golden.empty()) { std::cerr << "EMPTY FAIL\n"; std::abort(); }
    if (!golden.empty()) {
        if (il.front() != golden.front()) { std::cerr << "FRONT FAIL\n"; std::abort(); }
        if (il.back() != golden.back()) { std::cerr << "BACK FAIL\n"; std::abort(); }
    }
    auto it = golden.begin();
    auto idx = il.front_index();
    while (it != golden.end() && idx != IndexList<T>::npos) {
        if (il[idx] != *it) { std::cerr << "TRAVERSAL FAIL\n"; std::abort(); }
        ++it;
        auto opt = il.next_index(idx);
        idx = opt ? *opt : IndexList<T>::npos;
    }
    if (it != golden.end() || idx != IndexList<T>::npos) { std::cerr << "LEN FAIL\n"; std::abort(); }
}

// ====================================================================
//  Sync Wrappers
// ====================================================================
template<class T, class U> void sync_push_back(IndexList<T>& il, std::list<T>& l, U&& v) { il.push_back(std::forward<U>(v)); l.push_back(std::forward<U>(v)); }
template<class T, class U> void sync_push_front(IndexList<T>& il, std::list<T>& l, U&& v) { il.push_front(std::forward<U>(v)); l.push_front(std::forward<U>(v)); }
template<class T, class... A> void sync_emplace_back(IndexList<T>& il, std::list<T>& l, A&&... a) { il.emplace_back(std::forward<A>(a)...); l.emplace_back(std::forward<A>(a)...); }
template<class T, class... A> void sync_emplace_front(IndexList<T>& il, std::list<T>& l, A&&... a) { il.emplace_front(std::forward<A>(a)...); l.emplace_front(std::forward<A>(a)...); }
template<class T> void sync_pop_back(IndexList<T>& il, std::list<T>& l) { assert(!il.empty()); il.pop_back(); l.pop_back(); }
template<class T> void sync_pop_front(IndexList<T>& il, std::list<T>& l) { assert(!il.empty()); il.pop_front(); l.pop_front(); }
template<class T, class P> void sync_remove_if(IndexList<T>& il, std::list<T>& l, P p) { il.remove_if(p); l.remove_if(p); }

// ====================================================================
//  Stress Test Loop
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
            case 6: if (!il.empty() && rng()%2) sync_remove_if(il, golden, [&](const T& x){ return x % 7 == 0; }); break;
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
    stress_test<long>(seed);
    stress_test<Packet>(seed);
    std::cout << "All IndexList tests passed!\n";
    return 0;
}
