# Thin alias layer over CMake and the shell helpers in this repo.
# CMake stays the only build system; nothing here may duplicate its logic.

PRESET    ?= linux-debug
BUILD_DIR := build/$(PRESET)
NPROC     := $(shell nproc 2>/dev/null || echo 4)

# Terminal colors
RESET  := \033[0m
BOLD   := \033[1m
CYAN   := \033[36m
GREEN  := \033[32m
YELLOW := \033[33m
BLUE   := \033[34m
RED    := \033[31m
GRAY   := \033[90m

.PHONY: all dev release test smoke run dist clean distclean format help asan tsan memcheck

all: dev

## dev: configure + build debug (default target)
dev:
	@printf "$(BOLD)$(CYAN)==> Configuring$(RESET) $(BLUE)linux-debug$(RESET)\n"
	cmake --preset linux-debug
	@printf "$(BOLD)$(CYAN)==> Building$(RESET) $(BLUE)linux-debug$(RESET) $(GRAY)(jobs: $(NPROC))$(RESET)\n"
	cmake --build build/linux-debug -j$(NPROC)
	@printf "$(BOLD)$(GREEN)==> Debug build complete$(RESET)\n"

## release: configure + build release
release:
	@printf "$(BOLD)$(CYAN)==> Configuring$(RESET) $(BLUE)linux-release$(RESET)\n"
	cmake --preset linux-release
	@printf "$(BOLD)$(CYAN)==> Building$(RESET) $(BLUE)linux-release$(RESET) $(GRAY)(jobs: $(NPROC))$(RESET)\n"
	cmake --build build/linux-release -j$(NPROC)
	@printf "$(BOLD)$(GREEN)==> Release build complete$(RESET)\n"

## test: build and run unit tests, then ctest (unit + smoke)
test: dev
	@printf "$(BOLD)$(CYAN)==> Running unit tests$(RESET)\n"
	./$(BUILD_DIR)/tests
	@printf "$(BOLD)$(CYAN)==> Running CTest$(RESET)\n"
	cd $(BUILD_DIR) && ctest --output-on-failure
	@printf "$(BOLD)$(GREEN)==> All tests passed$(RESET)\n"

## smoke: build and run the 50-iteration hot-reload test
smoke: dev
	@printf "$(BOLD)$(CYAN)==> Running smoke test$(RESET)\n"
	./$(BUILD_DIR)/smoketest
	@printf "$(BOLD)$(GREEN)==> Smoke test passed$(RESET)\n"

## run: launch the release editor
run: release
	@printf "$(BOLD)$(CYAN)==> Launching$(RESET) $(BLUE)RayWaves$(RESET)\n"
	./build/linux-release/RayWaves

## dist: create ./dist distribution package
dist:
	@printf "$(BOLD)$(CYAN)==> Creating distribution$(RESET)\n"
	Distribution/distribute.sh -BuildConfig Release -OutputDir dist
	@printf "$(BOLD)$(GREEN)==> Distribution created$(RESET) $(GRAY)(./dist)$(RESET)\n"

## clean: remove build artifacts (keeps CMakeCache)
clean:
	@printf "$(BOLD)$(YELLOW)==> Cleaning$(RESET) $(BLUE)$(BUILD_DIR)$(RESET)\n"
	@if [ -d "$(BUILD_DIR)" ]; then \
		cmake --build $(BUILD_DIR) --target clean; \
	else \
		printf "$(GRAY)Nothing to clean: $(BUILD_DIR) does not exist$(RESET)\n"; \
	fi

## distclean: remove the whole build tree including CMakeCache.txt
distclean:
	@printf "$(BOLD)$(RED)==> Removing$(RESET) $(BLUE)build/$(RESET)\n"
	rm -rf build

## format: format first-party sources in-place
format:
	@printf "$(BOLD)$(CYAN)==> Formatting first-party sources$(RESET)\n"
	Tools/run_analysis.sh format --preset $(PRESET)
	@printf "$(BOLD)$(GREEN)==> Formatting complete$(RESET)\n"

## asan: ASan+UBSan build, run unit tests + smoke test with leak check
asan:
	@printf "$(BOLD)$(CYAN)==> Configuring $(BLUE)ASan/UBSan$(RESET)\n"
	cmake -S . -B build/asan -DCMAKE_BUILD_TYPE=Debug -DRAYWAVES_SANITIZERS=ON
	@printf "$(BOLD)$(CYAN)==> Building$(RESET)\n"
	cmake --build build/asan -j$(NPROC) --target tests smoketest main
	@printf "$(BOLD)$(CYAN)==> Running tests under ASan/LSan$(RESET)\n"
	cd build/asan && ASAN_OPTIONS=detect_leaks=1 ./tests
	@printf "$(BOLD)$(CYAN)==> Running smoke under ASan/LSan$(RESET)\n"
	cd build/asan && ASAN_OPTIONS=detect_leaks=1 ./smoketest
	@printf "$(BOLD)$(GREEN)==> ASan/UBSan clean$(RESET)\n"

## tsan: ThreadSanitizer build, run unit tests + smoke test
tsan:
	@printf "$(BOLD)$(CYAN)==> Configuring $(BLUE)ThreadSanitizer$(RESET)\n"
	cmake -S . -B build/tsan -DCMAKE_BUILD_TYPE=Debug -DRAYWAVES_TSAN=ON
	@printf "$(BOLD)$(CYAN)==> Building$(RESET)\n"
	cmake --build build/tsan -j$(NPROC) --target tests smoketest
	@printf "$(BOLD)$(CYAN)==> Running tests under TSan$(RESET)\n"
	cd build/tsan && TSAN_OPTIONS=halt_on_error=1 ./tests
	@printf "$(BOLD)$(CYAN)==> Running smoke under TSan$(RESET)\n"
	cd build/tsan && TSAN_OPTIONS=halt_on_error=1 ./smoketest
	@printf "$(BOLD)$(GREEN)==> TSan clean$(RESET)\n"

## memcheck: valgrind memcheck over unit tests (smoke covered by asan)
memcheck: dev
	@printf "$(BOLD)$(CYAN)==> Valgrind memcheck: tests$(RESET)\n"
	valgrind --error-exitcode=1 --leak-check=full --show-leak-kinds=definite \
		./$(BUILD_DIR)/tests
	@printf "$(BOLD)$(GREEN)==> Valgrind clean$(RESET)\n"

## help: display available targets and usage
help:
	@printf "\n"
	@printf "$(BOLD)$(CYAN)RayWaves Game Engine$(RESET)\n"
	@printf "$(GRAY)Build, test, development and distribution commands$(RESET)\n"
	@printf "\n"

	@printf "$(BOLD)Usage$(RESET)\n"
	@printf "  make $(CYAN)<target>$(RESET)\n"
	@printf "\n"

	@printf "$(BOLD)Build$(RESET)\n"
	@printf "  $(GREEN)dev$(RESET)          Configure and build the debug target\n"
	@printf "  $(GREEN)release$(RESET)      Configure and build the release target\n"
	@printf "  $(GREEN)run$(RESET)          Build release and launch RayWaves\n"
	@printf "\n"

	@printf "$(BOLD)Testing$(RESET)\n"
	@printf "  $(GREEN)test$(RESET)         Build and run unit tests + CTest\n"
	@printf "  $(GREEN)smoke$(RESET)        Build and run the hot-reload smoke test\n"
	@printf "\n"

	@printf "$(BOLD)Development$(RESET)\n"
	@printf "  $(GREEN)format$(RESET)       Format first-party sources\n"
	@printf "\n"

	@printf "$(BOLD)Analysis$(RESET)\n"
	@printf "  $(GREEN)asan$(RESET)         Run tests + smoke under ASan/UBSan/LSan\n"
	@printf "  $(GREEN)tsan$(RESET)         Run tests + smoke under ThreadSanitizer\n"
	@printf "  $(GREEN)memcheck$(RESET)     Run unit tests under valgrind\n"
	@printf "\n"

	@printf "$(BOLD)Distribution$(RESET)\n"
	@printf "  $(GREEN)dist$(RESET)         Create the release distribution package\n"
	@printf "\n"

	@printf "$(BOLD)Maintenance$(RESET)\n"
	@printf "  $(GREEN)clean$(RESET)        Remove build artifacts, keep CMake cache\n"
	@printf "  $(GREEN)distclean$(RESET)    Remove the complete build tree\n"
	@printf "\n"

	@printf "$(BOLD)Configuration$(RESET)\n"
	@printf "  $(GRAY)PRESET=$(PRESET)$(RESET)\n"
	@printf "  Override with: $(CYAN)make PRESET=linux-release <target>$(RESET)\n"
	@printf "\n"

	@printf "$(GRAY)────────────────────────────────────────────────────────────$(RESET)\n"
	@printf "$(BOLD)$(BLUE)Build system$(RESET)  $(GRAY)CMake$(RESET)\n"
	@printf "$(BOLD)$(BLUE)Makefile role$(RESET)  $(GRAY)Thin command alias layer$(RESET)\n"
	@printf "$(GRAY)────────────────────────────────────────────────────────────$(RESET)\n"
	@printf "\n"
