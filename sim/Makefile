# Makefile for D4K-3ch Anduril Simulator
# Tests REAL channel mode calculations from firmware
#
# Two modes:
#   1. Test harness (default) - runs real firmware C calculations
#   2. AVR emulator (requires simavr with ATtiny1634 core)

CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=gnu99 -g
LDFLAGS = -lm

# Test harness - uses REAL firmware channel mode code
TEST_SRCS = sim-test-harness.c sim-render.c
TEST_OBJS = $(TEST_SRCS:.c=.o)
TEST_TARGET = sim-d4k-3ch

# AVR emulator (requires libsimavr with ATtiny1634 support)
AVR_SRCS = sim-avr.c sim-render.c
AVR_OBJS = $(AVR_SRCS:.c=.o)
AVR_TARGET = sim-d4k-3ch-avr
AVR_LDFLAGS = -lsimavr -lpthread -lm

# Firmware hex file
HEX_FILE = ../hex/anduril.hank-emisar-d4k-3ch.hex

# Default target - build the test harness
all: $(TEST_TARGET)

# Build test harness (uses real firmware channel mode calculations)
$(TEST_TARGET): $(TEST_OBJS)
	$(CC) $(CFLAGS) -o $@ $(TEST_OBJS) $(LDFLAGS)

# Build AVR emulator (requires simavr to be installed with ATtiny1634 core)
avr: $(AVR_TARGET)

$(AVR_TARGET): $(AVR_OBJS)
	$(CC) $(CFLAGS) -o $@ $(AVR_OBJS) $(AVR_LDFLAGS)

# Compile rules
sim-test-harness.o: sim-test-harness.c sim-render.h
	$(CC) $(CFLAGS) -c $< -o $@

sim-avr.o: sim-avr.c sim-render.h
	$(CC) $(CFLAGS) -c $< -o $@

sim-render.o: sim-render.c sim-render.h
	$(CC) $(CFLAGS) -c $< -o $@

# Run targets
run: $(TEST_TARGET)
	./$(TEST_TARGET)

run-avr: $(AVR_TARGET) $(HEX_FILE)
	./$(AVR_TARGET) $(HEX_FILE)

# Build firmware first if needed
$(HEX_FILE):
	cd .. && ./make emisar-d4k-3ch

# Clean build artifacts
clean:
	rm -f *.o $(TEST_TARGET) $(AVR_TARGET)

# Debug build
debug: CFLAGS += -g -DDEBUG -O0
debug: clean $(TEST_TARGET)

# Help
help:
	@echo "D4K-3ch Anduril Simulator"
	@echo ""
	@echo "Targets:"
	@echo "  all (default)  Build test harness with real channel mode code"
	@echo "  avr            Build AVR emulator (requires simavr+ATtiny1634)"
	@echo "  run            Run test harness"
	@echo "  run-avr        Run AVR emulator with firmware hex"
	@echo "  clean          Remove build artifacts"
	@echo ""
	@echo "Controls:"
	@echo "  c     Click (toggle on/off)"
	@echo "  m     Cycle channel modes"
	@echo "  +/-   Adjust brightness"
	@echo "  [/]   Adjust tint/hue/CCT"
	@echo "  h     Hold (ramp)"
	@echo "  q     Quit"

.PHONY: all avr run run-avr clean debug help
