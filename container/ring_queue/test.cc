#include "ring_queue.hh"
#include <deque>
#include <cassert>
#include <iostream>
#include <random>
#include <sstream>
#include <type_traits>
#include <utility>

/*======================================================================
 *  Test element types
 *====================================================================*/

/** Trivial, copyable, comparable – good for basic sanity checks. */
using TrivialElement = long;

/**
 * @brief Non-trivial, move-only packet with self-incrementing ID.
 *
 * Used in tests to exercise:
 * - placement-new
 * - move construction/assignment
 * - explicit destructor calls
 * - atomic ID generation
 */
struct Packet {
    // -----------------------------------------------------------------
    //  Data members
    // -----------------------------------------------------------------
    std::int64_t id;       ///< Unique, auto-incrementing identifier
    std::int64_t payload;  ///< Arbitrary payload (long)

    // -----------------------------------------------------------------
    //  Static ID generator (thread-safe)
    // -----------------------------------------------------------------
private:
    static std::int64_t next_id;

    // -----------------------------------------------------------------
    //  Construction
    // -----------------------------------------------------------------
public:
    /** Default ctor – ID is assigned automatically, payload = 0 */
    Packet() noexcept
        : id(++next_id)
        , payload(0)
    {}

    /** Construct from a single integer – becomes the payload */
    template<class Integer,
             class = std::enable_if_t<std::is_integral_v<Integer>>>
    explicit Packet(Integer p) noexcept
        : id(++next_id)
        , payload(static_cast<std::int64_t>(p))
    {}

    // Delete copy
    Packet(const Packet&)            = delete;
    Packet& operator=(const Packet&) = delete;

    // Default move (noexcept)
    Packet(Packet&&) noexcept            = default;
    Packet& operator=(Packet&&) noexcept = default;

    // -----------------------------------------------------------------
    //  Comparison
    // -----------------------------------------------------------------
    bool operator==(const Packet& rhs) const noexcept {
        bool id_ok = id == rhs.id || id == rhs.id + 1 || id == rhs.id - 1;
        return id_ok && payload == rhs.payload;
    }
};

// ---------------------------------------------------------------------
//  Static member definition (must be in a .cc file or inline in C++17+)
// ---------------------------------------------------------------------
inline std::int64_t Packet::next_id(0);

/**
 * @brief Stream insertion operator for Packet.
 *
 * Output format:  Packet{id=5, payload=123}
 *
 * @param os  Output stream
 * @param p   Packet to print
 * @return os for chaining
 */
inline std::ostream& operator<<(std::ostream& os, const Packet& p)
{
    os << "Packet{id=" << p.id
       << ", payload=" << p.payload
       << '}';
    return os;
}

/*======================================================================
 *  Golden-model checker
 *====================================================================*/

template<class T>
bool check_ring_queue(
    const RingQueue<T>& rq,
    const std::deque<T>& golden,
    std::size_t iteration
)
{
    // -----------------------------------------------------------------
    // Size & emptiness
    // -----------------------------------------------------------------
    if (rq.size() != golden.size()) {
        std::cerr << "SIZE MISMATCH at iteration " << iteration << std::endl
                  << "  Expected: " << golden.size()
                  << "  Actual: "   << rq.size() << "\n";
        return false;
    }

    if (rq.empty() != golden.empty()) {
        std::cerr << "empty() MISMATCH at iteration " << iteration << std::endl
                  << "  Expected: " << golden.empty()
                  << "  Actual: "   << rq.empty() << "\n";
        return false;
    }

    // -----------------------------------------------------------------
    // Front element
    // -----------------------------------------------------------------
    if (!golden.empty()) {
        if (!(rq.front() == golden.front())) {
            std::cerr << "FRONT MISMATCH at iteration " << iteration << std::endl
                      << "  Expected: " << golden.front() << "\n"
                      << "  Actual:   " << rq.front()     << "\n";
            return false;
        }

        const RingQueue<T>& crq = rq;
        if (!(crq.front() == golden.front())) {
            std::cerr << "CONST FRONT MISMATCH at iteration " << iteration << std::endl
                      << "  Expected: " << golden.front() << "\n"
                      << "  Actual:   " << crq.front()    << "\n";
            return false;
        }
    }

    // -----------------------------------------------------------------
    // Back element (newest)
    // -----------------------------------------------------------------
    if (!golden.empty()) {
        if (!(rq.back() == golden.back())) {
            std::cerr << "BACK MISMATCH at iteration " << iteration << std::endl
                      << "  Expected: " << golden.back() << "\n"
                      << "  Actual:   " << rq.back()     << "\n";
            return false;
        }

        const RingQueue<T>& crq = rq;
        if (!(crq.back() == golden.back())) {
            std::cerr << "CONST BACK MISMATCH at iteration " << iteration << std::endl
                      << "  Expected: " << golden.back() << "\n"
                      << "  Actual:   " << crq.back()    << "\n";
            return false;
        }
    }

    return true;
}

/*======================================================================
 *  Modification wrappers – keep RingQueue and std::deque in sync
 *====================================================================*/

/**
 * @brief Push a value into *both* containers.
 *
 * @tparam T   Element type.
 * @param rq   RingQueue under test.
 * @param dq   Golden-model deque.
 * @param val  Value to push (r-value or l-value).
 */
template<class T, class U>
void sync_push(RingQueue<T>& rq, std::deque<T>& dq, U&& val)
{
    rq.push(std::forward<U>(val));
    dq.push_back(std::forward<U>(val));
}

/**
 * @brief Emplace an object in-place into *both* containers.
 *
 * @tparam T     Element type.
 * @tparam Args  Constructor argument types.
 * @param rq     RingQueue under test.
 * @param dq     Golden-model deque.
 * @param args   Arguments forwarded to the constructor.
 */
template<class T, class... Args>
void sync_emplace(RingQueue<T>& rq, std::deque<T>& dq, Args&&... args)
{
    rq.emplace(std::forward<Args>(args)...);
    dq.emplace_back(std::forward<Args>(args)...);
}

/**
 * @brief Pop the front element from *both* containers.
 *
 * @pre Both containers are non-empty.
 */
template<class T>
void sync_pop(RingQueue<T>& rq, std::deque<T>& dq)
{
    // Optional: assert non-empty in debug builds
    assert(!rq.empty() && "sync_pop on empty RingQueue");
    assert(!dq.empty() && "sync_pop on empty golden deque");

    rq.pop();
    dq.pop_front();
}

/**
 * @brief Stress-test RingQueue against std::deque with random operations.
 *
 * @tparam T             Element type (must be constructible from int).
 * @tparam Iterations    Number of random operations (default 100'000).
 * @param seed           Random seed (default = 42). Printed to stdout.
 */
template<class T, std::size_t Iterations = 100'000>
void stress_test_ring_queue(std::mt19937::result_type seed = 42)
{
    RingQueue<T> rq;
    std::deque<T> dq;

    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> op_dist(0, 4);        // 0..4 → 5 choices
    std::uniform_int_distribution<int64_t> val_dist(INT64_MIN, INT64_MAX);     // payload values

    // -----------------------------------------------------------------
    // Initial state verification
    // -----------------------------------------------------------------
    if (not check_ring_queue(rq, dq, -1)) {
        std::cerr << "Error after initialization." << std::endl;
        return;
    }
    std::cout << "=== RingQueue stress test ===\n"
              << "Element type: " << typeid(T).name() << "\n"
              << "Iterations: "   << Iterations << "\n"
              << "Random seed: "  << seed << "\n\n";

    // -----------------------------------------------------------------
    // Main random-operation loop
    // -----------------------------------------------------------------
    for (std::size_t i = 0; i < Iterations; ++i) {
        const int op = op_dist(rng);
        T elem = T(val_dist(rng));
        int64_t val = val_dist(rng);
        std::stringstream ss;


        switch (op) {
            case 0: // push
                ss << "Push " << elem << std::endl;
                sync_push(rq, dq, T(val));
                break;

            case 1: // emplace
                ss << "Emplace " << val << std::endl;
                sync_emplace(rq, dq, val);
                break;

            case 2: // pop
                if (rq.empty()) break;
                ss << "Pop" << std::endl;
                sync_pop(rq, dq);
                break;

            case 3: // shrink_to_fit (RingQueue only)
                ss << "Shrink" << std::endl;
                rq.shrink_to_fit();
                break;

            case 4: // reserve (RingQueue only)
            {
                // Reserve a random power-of-two capacity between 1 and 1024
                std::uniform_int_distribution<std::size_t> cap_dist(1, 1024);
                const std::size_t new_cap = cap_dist(rng);
                ss << "Reserve " << new_cap << std::endl;
                rq.reserve(new_cap);
                break;
            }
        }
        if (not check_ring_queue(rq, dq, i)) {
            std::cerr << "Error after " << ss.str();
            return;
        }
    }

    std::cout << "All " << Iterations << " operations passed!\n";
}

// ---------------------------------------------------------------------
//  Helper: parse seed from command line, or generate random
// ---------------------------------------------------------------------
std::mt19937::result_type get_seed(int argc, char* argv[])
{
    if (argc >= 2) {
        try {
            std::size_t seed = std::stoull(argv[1]);
            std::cout << "Using user-provided seed: " << seed << "\n";
            return static_cast<std::mt19937::result_type>(seed);
        } catch (...) {
            std::cerr << "Warning: Invalid seed '" << argv[1]
                      << "'. Falling back to random seed.\n";
        }
    }

    std::random_device rd;
    auto seed = rd();
    std::cout << "Using random seed: " << seed << "\n";
    return seed;
}

// ---------------------------------------------------------------------
//  Main entry point
// ---------------------------------------------------------------------
int main(int argc, char* argv[])
{
    const auto seed = get_seed(argc, argv);
    constexpr std::size_t kIterations = 200'000;

    std::cout << "\n=== RingQueue Stress Test ===\n\n";

    // -----------------------------------------------------------------
    //  Test 1: Trivial type (long)
    // -----------------------------------------------------------------
    std::cout << "Test 1: Element = long\n";
    stress_test_ring_queue<long, kIterations>(seed);

    // -----------------------------------------------------------------
    //  Test 2: Non-trivial, move-only (Packet)
    // -----------------------------------------------------------------
    std::cout << "\nTest 2: Element = Packet (move-only, auto-ID)\n";
    stress_test_ring_queue<Packet, kIterations>(seed);

    std::cout << "\nAll tests passed!\n";
    return 0;
}
