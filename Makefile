# Thin alias layer over CMake and the shell helpers in this repo.
# CMake stays the only build system; nothing here may duplicate its logic.
PRESET   ?= linux-debug
BUILD_DIR := build/$(PRESET)
NPROC     := $(shell nproc 2>/dev/null || echo 4)

.PHONY: all dev release test smoke run dist clean distclean format tidy help

all: dev

## dev: configure + build debug (default target)
dev:
	cmake --preset linux-debug
	cmake --build build/linux-debug -j$(NPROC)

## release: configure + build release
release:
	cmake --preset linux-release
	cmake --build build/linux-release -j$(NPROC)

## test: build and run unit tests, then ctest (unit + smoke)
test: dev
	./$(BUILD_DIR)/tests
	cd $(BUILD_DIR) && ctest --output-on-failure

## smoke: build and run the 50-iteration hot-reload test
smoke: dev
	./$(BUILD_DIR)/smoketest

## run: launch the release editor
run: release
	./build/linux-release/RayWaves

## dist: create ./dist distribution package
dist:
	Distribution/distribute.sh -BuildConfig Release -OutputDir dist

## clean: remove build artifacts (keeps CMakeCache)
clean:
	@if [ -d "$(BUILD_DIR)" ]; then cmake --build $(BUILD_DIR) --target clean; fi

## distclean: remove the whole build tree including CMakeCache.txt
distclean:
	rm -rf build

## format: format first-party sources in-place
format:
	Tools/run_analysis.sh format --preset $(PRESET)

## tidy: clang-tidy over first-party sources
tidy:
	Tools/run_analysis.sh tidy --preset $(PRESET)

## help: list targets
help:
	@echo "RayWaves Game Engine - make targets:"
	@grep -E '^## ' Makefile | sed 's/^## /  /'
	@echo
	@echo "  PRESET=$(PRESET) (override: make PRESET=linux-release <target>)"
