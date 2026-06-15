# Compile-checks the reference interface skeleton (not the firmware).
# Proves the HAL + device-service contracts form a consistent, buildable set.
CC      ?= cc
CFLAGS  ?= -std=c99 -Wall -Wextra -Wpedantic -Iinclude
BUILD   ?= build

.PHONY: all check clean
all: check

check: $(BUILD)/build_check
	$(BUILD)/build_check

$(BUILD)/build_check: build-check/build_check.c $(wildcard include/**/*.h) | $(BUILD)
	$(CC) $(CFLAGS) build-check/build_check.c -o $@

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)
