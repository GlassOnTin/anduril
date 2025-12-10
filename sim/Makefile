# Makefile for D4K-3ch Anduril Simulator
# Tests REAL channel mode calculations from firmware

CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=gnu99 -g
LDFLAGS = -lm

# Test harness - uses REAL firmware channel mode code
TEST_SRCS = sim-test-harness.c sim-render.c
TEST_OBJS = $(TEST_SRCS:.c=.o)
TEST_TARGET = sim-d4k-3ch

# Standalone demo (for testing rendering only)
DEMO_SRCS = sim-main.c sim-hwdef.c sim-render.c
DEMO_OBJS = $(DEMO_SRCS:.c=.o)
DEMO_TARGET = sim-d4k-3ch-demo

# Default target - build the test harness
all: $(TEST_TARGET)

# Build the test harness (uses real firmware channel mode calculations)
$(TEST_TARGET): $(TEST_OBJS)
	$(CC) $(CFLAGS) -o $@ $(TEST_OBJS) $(LDFLAGS)

# Build the standalone demo
demo: $(DEMO_TARGET)

$(DEMO_TARGET): $(DEMO_OBJS)
	$(CC) $(CFLAGS) -o $@ $(DEMO_OBJS) $(LDFLAGS)

# Compile rules
sim-test-harness.o: sim-test-harness.c sim-render.h
	$(CC) $(CFLAGS) -c $< -o $@

sim-render.o: sim-render.c sim-render.h
	$(CC) $(CFLAGS) -c $< -o $@

sim-main.o: sim-main.c sim-hwdef.h sim-render.h
	$(CC) $(CFLAGS) -c $< -o $@

sim-hwdef.o: sim-hwdef.c sim-hwdef.h
	$(CC) $(CFLAGS) -c $< -o $@

# Run targets
run: $(TEST_TARGET)
	./$(TEST_TARGET)

run-demo: $(DEMO_TARGET)
	./$(DEMO_TARGET)

# Clean build artifacts
clean:
	rm -f *.o $(TEST_TARGET) $(DEMO_TARGET)

# Debug build
debug: CFLAGS += -g -DDEBUG -O0
debug: clean $(TEST_TARGET)

.PHONY: all demo run run-demo clean debug
