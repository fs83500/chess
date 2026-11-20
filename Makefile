# Automated Chess Board - build helpers for environments without PlatformIO.
# The ESP32 firmware is normally built with:  pio run -e esp32dev
# These targets let you compile-check and test the engine on a plain host.

CXX      ?= g++
CXXFLAGS ?= -std=gnu++17 -O2 -DUNIT_TEST -Isrc

SRC      := src/chess/chess.cpp
TEST     := src/host_tests.cpp

.PHONY: test clean check-engine

test: /tmp/chess_tests
	/tmp/chess_tests

/tmp/chess_tests: $(SRC) $(TEST)
	$(CXX) $(CXXFLAGS) $(SRC) $(TEST) -o /tmp/chess_tests

# Compile-check the engine + headers only (no test runner).
check-engine: $(SRC) src/chess/chess.h
	$(CXX) $(CXXFLAGS) -c $(SRC) -o /tmp/chess.o

clean:
	rm -f /tmp/chess_tests /tmp/chess.o
