# container/ring_queue/Makefile
CXX      := g++
CXXFLAGS := -std=c++17 -O3 -Wall -Wextra -march=native -I.
OPTFLAGS := -O3
DBGFLAGS := -O0 -g3 -fno-inline -fno-omit-frame-pointer -DDEBUG_PRINT

# Test
T_TARGET := test.run
T_SRCS   := test.cc

test: $(T_TARGET)
	@echo "=== Running correctness test ==="
	./$(T_TARGET)

$(T_TARGET): $(T_SRCS)
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) -o $@ $^

# Debug
D_TARGET := test.debug
debug: $(D_TARGET)
	@echo "=== Launching gdb ==="
	gdb ./$(D_TARGET)

$(D_TARGET): $(T_SRCS)
	$(CXX) $(CXXFLAGS) $(DBGFLAGS) -o $@ $^

# Performance
P_TARGET := perf.run
P_SRCS   := perf.cc

perf: $(P_TARGET)
	@echo "=== Running performance benchmark ==="
	./$(P_TARGET)

$(P_TARGET): $(P_SRCS)
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) -o $@ $^

clean:
	rm -f $(T_TARGET) $(D_TARGET) $(P_TARGET) *.o

.PHONY: test debug perf clean
