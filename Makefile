# Makefile for ISO 14229-1 UDS Stack
# Supports macOS and Linux

CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Iinclude
LDFLAGS :=

# Sanitizer flags (use with 'make sanitize')
SANITIZE_FLAGS := -fsanitize=address,undefined -fno-omit-frame-pointer -g
COVERAGE_FLAGS := --coverage -O0 

# Google Test configuration
# Install: brew install googletest (macOS) or apt install libgtest-dev (Linux)
GTEST_CFLAGS := $(shell pkg-config --cflags gtest 2>/dev/null || echo "")
GTEST_LIBS := $(shell pkg-config --libs gtest gtest_main 2>/dev/null || echo "-lgtest -lgtest_main -pthread")
GTEST_AVAILABLE := $(shell pkg-config --exists gtest 2>/dev/null && echo "yes" || echo "no")

# Coverage flags (use: make coverage)
COVERAGE_FLAGS := -fprofile-arcs -ftest-coverage
COVERAGE_LIBS := -lgcov

# Directories
SRC_DIR := src
INC_DIR := include
OBJ_DIR := build
EXAMPLES_DIR := examples
TESTS_DIR := tests
GTEST_DIR := tests/gtest
BIN_DIR := bin
TEST_BIN_DIR := bin/tests
COVERAGE_DIR := coverage

# Source files
SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

# Example files
EXAMPLE_SRCS := $(wildcard $(EXAMPLES_DIR)/*.cpp)
EXAMPLE_BINS := $(EXAMPLE_SRCS:$(EXAMPLES_DIR)/%.cpp=$(BIN_DIR)/%)

# Test files (legacy custom framework)
TEST_SRCS := $(wildcard $(TESTS_DIR)/*.cpp)
TEST_BINS := $(TEST_SRCS:$(TESTS_DIR)/%.cpp=$(TEST_BIN_DIR)/%)

# Google Test files
GTEST_SRCS := $(wildcard $(GTEST_DIR)/*.cpp)
GTEST_BINS := $(GTEST_SRCS:$(GTEST_DIR)/%.cpp=$(TEST_BIN_DIR)/gtest_%)

# Library
LIB := libuds.a

# Targets
.PHONY: all lib examples tests gtest clean dirs test run-tests run-gtest coverage coverage-report sanitize afl-build afl-fuzz

all: dirs lib examples

dirs:
	@mkdir -p $(OBJ_DIR) $(BIN_DIR) $(TEST_BIN_DIR)

lib: dirs $(OBJ_DIR)/$(LIB)

$(OBJ_DIR)/$(LIB): $(OBJS)
	@echo "Creating static library: $@"
	ar rcs $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@echo "Compiling: $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@

examples: dirs lib $(EXAMPLE_BINS)

$(BIN_DIR)/%: $(EXAMPLES_DIR)/%.cpp $(OBJ_DIR)/$(LIB)
	@echo "Building example: $@"
	$(CXX) $(CXXFLAGS) $< $(OBJ_DIR)/$(LIB) $(LDFLAGS) -o $@

# Test targets (legacy framework)
tests: dirs lib $(TEST_BINS)

$(TEST_BIN_DIR)/%: $(TESTS_DIR)/%.cpp $(OBJ_DIR)/$(LIB)
	@echo "Building test: $@"
	$(CXX) $(CXXFLAGS) $< $(OBJ_DIR)/$(LIB) $(LDFLAGS) -o $@

# Google Test targets
gtest: dirs lib $(GTEST_BINS)
	@if [ "$(GTEST_AVAILABLE)" != "yes" ]; then \
		echo ""; \
		echo "⚠️  Google Test not found. Install with:"; \
		echo "    macOS:  brew install googletest"; \
		echo "    Ubuntu: sudo apt install libgtest-dev"; \
		echo ""; \
	fi

$(TEST_BIN_DIR)/gtest_%: $(GTEST_DIR)/%.cpp $(OBJ_DIR)/$(LIB)
	@echo "Building gtest: $@"
	$(CXX) $(CXXFLAGS) $(GTEST_CFLAGS) $< $(OBJ_DIR)/$(LIB) $(GTEST_LIBS) $(LDFLAGS) -o $@

# Run Google Tests
run-gtest: gtest
	@echo ""
	@echo "╔═══════════════════════════════════════════════════════════╗"
	@echo "║           Running Google Test Suite                       ║"
	@echo "╚═══════════════════════════════════════════════════════════╝"
	@echo ""
	@for test in $(GTEST_BINS); do \
		if [ -f "$$test" ]; then \
			echo "Running: $$test"; \
			./$$test --gtest_color=yes || exit 1; \
			echo ""; \
		fi \
	done

# Coverage build (requires gcov)
coverage: CXXFLAGS += $(COVERAGE_FLAGS)
coverage: LDFLAGS += $(COVERAGE_LIBS)
coverage: clean gtest
	@echo "Running tests with coverage..."
	@mkdir -p $(COVERAGE_DIR)
	@for test in $(GTEST_BINS); do \
		if [ -f "$$test" ]; then \
			./$$test || true; \
		fi \
	done
	@echo "Generating coverage report..."
	@gcov -o $(OBJ_DIR) $(SRCS) 2>/dev/null || true
	@mv *.gcov $(COVERAGE_DIR)/ 2>/dev/null || true
	@echo "Coverage files in $(COVERAGE_DIR)/"

# HTML coverage report (requires lcov)
coverage-report: coverage
	@echo "Generating HTML coverage report..."
	@lcov --capture --directory $(OBJ_DIR) --output-file $(COVERAGE_DIR)/coverage.info 2>/dev/null || \
		echo "lcov not installed. Install with: brew install lcov"
	@genhtml $(COVERAGE_DIR)/coverage.info --output-directory $(COVERAGE_DIR)/html 2>/dev/null || true
	@echo "HTML report: $(COVERAGE_DIR)/html/index.html"

# Run all tests
run-tests: tests
	@echo ""
	@echo "╔═══════════════════════════════════════════════════════════╗"
	@echo "║        Running UDS Test Suite - Phase 7 Compliance       ║"
	@echo "╚═══════════════════════════════════════════════════════════╝"
	@echo ""
	@for test in $(TEST_BINS); do \
		echo ""; \
		./$$test || exit 1; \
		echo ""; \
	done
	@echo ""
	@echo "╔═══════════════════════════════════════════════════════════╗"
	@echo "║              All Tests Completed Successfully             ║"
	@echo "╚═══════════════════════════════════════════════════════════╝"
	@echo ""

# Run individual test suites
test-core: tests
	@$(TEST_BIN_DIR)/test_uds_core

test-isotp: tests
	@$(TEST_BIN_DIR)/test_isotp

test-services: tests
	@$(TEST_BIN_DIR)/test_services

test-timing: tests
	@$(TEST_BIN_DIR)/test_timing

test-security: tests
	@$(TEST_BIN_DIR)/test_security

test-compliance: tests
	@$(TEST_BIN_DIR)/test_compliance

test-fuzzing: tests
	@$(TEST_BIN_DIR)/test_fuzzing

# Fuzzing target (extended run)
fuzz: tests
	@echo ""
	@echo "╔═══════════════════════════════════════════════════════════╗"
	@echo "║          Running Fuzzing Tests - Robustness Check        ║"
	@echo "╚═══════════════════════════════════════════════════════════╝"
	@echo ""
	@$(TEST_BIN_DIR)/test_fuzzing
	@echo ""
	@echo "Fuzzing complete - No crashes detected ✓"

# Convenience target
test: run-tests

# ============================================================================
# Sanitizer Builds (AddressSanitizer + UndefinedBehaviorSanitizer)
# ============================================================================

sanitize: CXXFLAGS += $(SANITIZE_FLAGS)
sanitize: LDFLAGS += $(SANITIZE_FLAGS)
sanitize: clean dirs lib tests
	@echo ""
	@echo "╔═══════════════════════════════════════════════════════════╗"
	@echo "║        Running Tests with AddressSanitizer + UBSan       ║"
	@echo "╚═══════════════════════════════════════════════════════════╝"
	@echo ""
	@for test in $(TEST_BINS); do \
		echo "Running $$test with sanitizers..."; \
		./$$test || exit 1; \
	done
	@echo ""
	@echo "✓ All sanitizer tests passed!"
	@echo ""

# ============================================================================
# AFL++ Fuzzing Support
# ============================================================================

afl-build:
	@echo "Building AFL++ fuzzing target..."
	@which afl-g++ > /dev/null || (echo "Error: AFL++ not found. Install with: brew install afl++" && exit 1)
	@mkdir -p $(BIN_DIR)
	afl-g++ -std=c++17 -Wall -Wextra -Iinclude \
		tests/fuzz_target_afl.cpp $(wildcard src/*.cpp) \
		-o $(BIN_DIR)/fuzz_afl
	@echo "AFL++ target built: $(BIN_DIR)/fuzz_afl"
	@echo ""
	@echo "To run AFL++ fuzzing:"
	@echo "  mkdir -p fuzz_findings"
	@echo "  afl-fuzz -i fuzz_seeds/ -o fuzz_findings/ $(BIN_DIR)/fuzz_afl"

afl-fuzz: afl-build
	@echo "Starting AFL++ fuzzing session..."
	@mkdir -p fuzz_findings
	@echo ""
	@echo "Running: afl-fuzz -i fuzz_seeds/ -o fuzz_findings/ $(BIN_DIR)/fuzz_afl"
	@echo "Press Ctrl+C to stop fuzzing"
	@echo ""
	afl-fuzz -i fuzz_seeds/ -o fuzz_findings/ $(BIN_DIR)/fuzz_afl

# ============================================================================
# Coverage Analysis
# ============================================================================

coverage: CXXFLAGS += $(COVERAGE_FLAGS)
coverage: LDFLAGS += $(COVERAGE_FLAGS)
coverage: clean dirs lib tests
	@echo "Running tests with coverage..."
	@for test in $(TEST_BINS); do \
		./$$test || exit 1; \
	done
	@echo "Generating coverage report..."
	@which lcov > /dev/null || (echo "Install lcov: brew install lcov" && exit 1)
	@lcov --capture --directory $(OBJ_DIR) --output-file coverage.info
	@lcov --remove coverage.info '/usr/*' --output-file coverage.info
	@lcov --list coverage.info
	@echo ""
	@echo "Coverage data generated: coverage.info"
	@echo "Generate HTML report with: genhtml coverage.info -o coverage_html/"

clean:
	@echo "Cleaning build artifacts"
	rm -rf $(OBJ_DIR) $(BIN_DIR) *.gcda *.gcno *.gcov coverage.info coverage_html/ fuzz_findings/

# Help target
help:
	@echo "ISO 14229-1 UDS Stack Build System - Phase 7"
	@echo ""
	@echo "Targets:"
	@echo "  all             - Build library and examples (default)"
	@echo "  lib             - Build static library only"
	@echo "  examples        - Build example programs"
	@echo "  tests           - Build legacy test suites"
	@echo "  run-tests       - Build and run legacy tests"
	@echo "  test            - Alias for run-tests"
	@echo ""
	@echo "Google Test targets:"
	@echo "  gtest           - Build Google Test suites"
	@echo "  run-gtest       - Build and run Google Tests"
	@echo "  coverage        - Build with coverage and run tests"
	@echo "  coverage-report - Generate HTML coverage report"
	@echo ""
	@echo "Legacy test targets:"
	@echo "  test-core       - Run core UDS tests only"
	@echo "  test-isotp      - Run ISO-TP tests only"
	@echo "  test-services   - Run service validation tests"
	@echo ""
	@echo "Other:"
	@echo "  clean           - Remove all build artifacts"
	@echo "  help            - Show this help message"
	@echo ""
	@echo "Usage:"
	@echo "  make                # Build everything"
	@echo "  make lib            # Build library only"
	@echo "  make gtest          # Build Google Tests"
	@echo "  make run-gtest      # Run Google Tests"
	@echo "  make coverage       # Run tests with coverage"
	@echo "  make clean          # Clean"
	@echo ""
	@echo "Prerequisites:"
	@echo "  Google Test: brew install googletest (macOS)"
	@echo "  Coverage:    brew install lcov (for HTML reports)"

# Dependency tracking
-include $(OBJS:.o=.d)

$(OBJ_DIR)/%.d: $(SRC_DIR)/%.cpp
	@$(CXX) $(CXXFLAGS) -MM -MT $(@:.d=.o) $< > $@
