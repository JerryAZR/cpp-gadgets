# **cpp-gadgets**  
*Lightweight C++ utilities for performance-critical code*  

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)  

---

## Overview

`cpp-gadgets` is a personal collection of **small, fast, and cache-conscious C++ utilities**.  

Currently focused on **gem5 modeling** (where cycle count and memory layout matter), but designed to be **general-purpose** and useful in any domain requiring low-latency or high-throughput data structures — think hardware simulation, game engines, or embedded systems.

> **Note**: These are **not drop-in STL replacements**.  
> They trade features (like iterator stability) for **speed and predictability**.  
> Use the STL when you need generality. Use this when you need performance.

---

## C++ Standard

- **gem5-targeted gadgets**: **C++17** (matches gem5’s build system)  
- **Other gadgets**: May use **C++20** or later (specified per-directory)

---

## Project Structure

```
cpp-gadgets/
└── container/
    └── ring_queue/
        ├── README.md
        ├── ring_queue.hh
        ├── main.cc          # performance benchmark vs STL
        ├── test.cc          # correctness / unit tests
        └── Makefile
```

- **`README.md`** – Purpose, design rationale, trade-offs  
- **`*.hh`** – Header-only implementation  
- **`main.cc`** – Performance comparison vs STL + usage examples  
- **`test.cc`** – Correctness and edge-case tests  
- **`Makefile`** – Build and run both `main` and `test`

---

## Current Gadgets

| Path | Description | STL Equivalent | C++ Standard |
|------|-------------|----------------|--------------|
| `container/ring_queue` | Vector-backed, contiguous ring buffer | `std::deque` | C++17 |
| `container/fast_list` | Index-linked list with O(1) middle removal | `std::list` | C++17 |

---

## Build & Run

Each gadget includes a `Makefile`:

```bash
cd container/ring_queue
make perf     # runs main.cc (performance)
make test     # runs test.cc (correctness)
make all      # both
make clean
```

---

## Contributing

**Work in progress.**  
Please open an **issue** first to discuss:
- New gadgets
- Design trade-offs
- Performance goals
- C++ standard version

PRs welcome after discussion.

---

## License

[MIT License](LICENSE) — do whatever you want with it.

---

*Just a repo for things I build. Mostly for gem5 right now. Might grow.*
