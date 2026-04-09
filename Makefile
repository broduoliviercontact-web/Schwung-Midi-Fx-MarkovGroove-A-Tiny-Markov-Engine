# Makefile — Schwung MIDI FX Module
#
# Usage:
#   make test          — build and run all native tests
#   make native        — build build/native/dsp.so
#   make aarch64       — build build/aarch64/dsp.so for Move (requires cross-compiler)
#   make check-symbols — verify move_midi_fx_init is exported
#   make clean         — remove all build artifacts

# --- Compiler flags -----------------------------------------------------------

CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -Werror -O2 -fPIC
INCLUDES = -Isrc/dsp -Isrc/host

# --- Source discovery ---------------------------------------------------------
# Collect all engine and host wrapper sources automatically.
# New modules: add _engine.c and _plugin.c to src/dsp/ and src/host/.

DSP_SRCS  := $(wildcard src/dsp/*_engine.c)
HOST_SRCS := $(wildcard src/host/*_plugin.c)
ALL_SRCS  := $(DSP_SRCS) $(HOST_SRCS)

# --- Test discovery -----------------------------------------------------------
# Engine tests link DSP sources only (no Schwung headers needed).
# MIDI FX tests link all sources (engine + host wrapper).

TEST_ENGINE_SRCS := $(wildcard tests/*_engine_test.c)
TEST_MIDI_SRCS   := $(wildcard tests/*_midi_fx_test.c)
TEST_ENGINE_BINS := $(patsubst tests/%.c, build/tests/%, $(TEST_ENGINE_SRCS))
TEST_MIDI_BINS   := $(patsubst tests/%.c, build/tests/%, $(TEST_MIDI_SRCS))
TEST_BINS        := $(TEST_ENGINE_BINS) $(TEST_MIDI_BINS)

# --- Build directories --------------------------------------------------------

$(shell mkdir -p build/native build/aarch64 build/tests)

# ==============================================================================
# Targets
# ==============================================================================

.PHONY: all test native aarch64 check-symbols clean

all: test

# --- Native tests -------------------------------------------------------------

test: $(TEST_BINS)
	@echo "Running native tests..."
	@failed=0; \
	for t in $(TEST_BINS); do \
		echo "  $$t"; \
		$$t || failed=$$((failed + 1)); \
	done; \
	if [ $$failed -eq 0 ]; then \
		echo "All tests passed."; \
	else \
		echo "$$failed test(s) FAILED."; exit 1; \
	fi

build/tests/%_engine_test: tests/%_engine_test.c $(DSP_SRCS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $< $(DSP_SRCS) -lm

build/tests/%_midi_fx_test: tests/%_midi_fx_test.c $(ALL_SRCS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $< $(ALL_SRCS) -lm

# --- Native dsp.so ------------------------------------------------------------

native: build/native/dsp.so

build/native/dsp.so: $(ALL_SRCS)
	$(CC) $(CFLAGS) $(INCLUDES) -shared -o $@ $(ALL_SRCS)

# --- aarch64 dsp.so (cross-compile) ------------------------------------------
# Tries: aarch64-linux-gnu-gcc, aarch64-linux-musl-gcc, zig cc

CROSS_CC := $(shell \
	command -v aarch64-linux-gnu-gcc  2>/dev/null || \
	command -v aarch64-linux-musl-gcc 2>/dev/null || \
	echo "")

aarch64: build/aarch64/dsp.so

build/aarch64/dsp.so: $(ALL_SRCS)
ifeq ($(CROSS_CC),)
	@echo "No aarch64 cross-compiler found. Trying zig cc..."
	zig cc -target aarch64-linux-gnu $(CFLAGS) $(INCLUDES) -shared -o $@ $(ALL_SRCS)
else
	$(CROSS_CC) $(CFLAGS) $(INCLUDES) -shared -o $@ $(ALL_SRCS)
endif

# --- Symbol check -------------------------------------------------------------

check-symbols: build/native/dsp.so
	@echo "Checking for move_midi_fx_init..."
	@nm build/native/dsp.so | grep -q "move_midi_fx_init" && \
		echo "  OK: move_midi_fx_init found" || \
		(echo "  FAIL: move_midi_fx_init NOT found"; exit 1)

# --- Clean --------------------------------------------------------------------

clean:
	rm -rf build/
