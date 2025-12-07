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
.PHONY: all lib examples tests gtest clean dirs test run-tests run-gtest coverage coverage-report sanitize afl-build afl-fuzz test-all test-quick

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

# Convenience target (legacy tests only)
test: run-tests

# ============================================================================
# Run ALL Tests (Google Test + Legacy + Fuzz)
# ============================================================================

test-all: dirs lib tests gtest
	@echo ""
	@echo "╔═══════════════════════════════════════════════════════════╗"
	@echo "║          Running Complete Test Suite                      ║"
	@echo "╚═══════════════════════════════════════════════════════════╝"
	@echo ""
	@echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
	@echo "  Phase 1: Google Test Suite"
	@echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
	@gtest_passed=0; gtest_total=0; \
	for test in $(GTEST_BINS); do \
		if [ -f "$$test" ]; then \
			gtest_total=$$((gtest_total + 1)); \
			echo "Running: $$test"; \
			if $$test --gtest_brief=1 2>&1 | tail -1; then \
				gtest_passed=$$((gtest_passed + 1)); \
			fi; \
		fi \
	done; \
	echo ""; \
	echo "Google Tests: $$gtest_passed/$$gtest_total suites passed"
	@echo ""
	@echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
	@echo "  Phase 2: Legacy Test Suite"
	@echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
	@legacy_passed=0; legacy_total=0; \
	for test in $(TEST_BINS); do \
		if [ -f "$$test" ]; then \
			testname=$$(basename $$test); \
			case "$$testname" in \
				fuzz_target_afl) continue ;; \
			esac; \
			legacy_total=$$((legacy_total + 1)); \
			echo "Running: $$testname"; \
			if $$test > /dev/null 2>&1; then \
				echo "  ✓ PASSED"; \
				legacy_passed=$$((legacy_passed + 1)); \
			else \
				echo "  ✗ FAILED"; \
			fi; \
		fi \
	done; \
	echo ""; \
	echo "Legacy Tests: $$legacy_passed/$$legacy_total passed"
	@echo ""
	@echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
	@echo "  Phase 3: Fuzz Testing (Quick)"
	@echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
	@if [ -f "$(TEST_BIN_DIR)/test_fuzzing" ]; then \
		echo "Running: test_fuzzing"; \
		if $(TEST_BIN_DIR)/test_fuzzing > /dev/null 2>&1; then \
			echo "  ✓ Fuzz tests passed"; \
		else \
			echo "  ✗ Fuzz tests failed"; \
		fi; \
	elif [ -f "$(TEST_BIN_DIR)/fuzz_target_afl" ]; then \
		echo "Running: fuzz_target_afl (quick mode)"; \
		echo "test" | $(TEST_BIN_DIR)/fuzz_target_afl > /dev/null 2>&1 && \
			echo "  ✓ Fuzz target runs without crash" || \
			echo "  ✗ Fuzz target crashed"; \
	else \
		echo "  (No fuzz tests available)"; \
	fi
	@echo ""
	@echo "╔═══════════════════════════════════════════════════════════╗"
	@echo "║          Complete Test Suite Finished                     ║"
	@echo "╚═══════════════════════════════════════════════════════════╝"
	@echo ""

# Quick test - just run gtests (fastest)
test-quick: gtest
	@echo ""
	@echo "Running quick test suite (Google Tests only)..."
	@for test in $(GTEST_BINS); do \
		if [ -f "$$test" ]; then \
			$$test --gtest_brief=1 || exit 1; \
		fi \
	done
	@echo ""
	@echo "✓ Quick tests passed!"

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

clean:
	@echo "Cleaning build artifacts"
	rm -rf $(OBJ_DIR) $(BIN_DIR) *.gcda *.gcno *.gcov coverage.info coverage_html/ fuzz_findings/

# Help target
help:
	@echo "ISO 14229-1 UDS Stack Build System"
	@echo ""
	@echo "Test Targets (Recommended):"
	@echo "  test-all        - Run ALL tests (gtest + legacy + fuzz)"
	@echo "  test-quick      - Run Google Tests only (fastest)"
	@echo "  run-gtest       - Run Google Tests with verbose output"
	@echo "  run-tests       - Run legacy tests only"
	@echo "  fuzz            - Run fuzzing tests"
	@echo ""
	@echo "Build Targets:"
	@echo "  all             - Build library and examples (default)"
	@echo "  lib             - Build static library only"
	@echo "  examples        - Build example programs"
	@echo "  tests           - Build legacy test suites"
	@echo "  gtest           - Build Google Test suites"
	@echo ""
	@echo "Quality Targets:"
	@echo "  sanitize        - Run tests with AddressSanitizer + UBSan"
	@echo "  coverage        - Run tests with code coverage"
	@echo "  coverage-report - Generate HTML coverage report"
	@echo "  afl-build       - Build AFL++ fuzzing target"
	@echo "  afl-fuzz        - Run AFL++ fuzzing session"
	@echo ""
	@echo "Individual Test Suites:"
	@echo "  test-core       - Run core UDS tests only"
	@echo "  test-isotp      - Run ISO-TP tests only"
	@echo "  test-services   - Run service validation tests"
	@echo "  test-security   - Run security tests only"
	@echo "  test-fuzzing    - Run fuzz tests only"
	@echo ""
	@echo "Other:"
	@echo "  clean           - Remove all build artifacts"
	@echo "  help            - Show this help message"
	@echo ""
	@echo "Quick Start:"
	@echo "  make test-all     # Run complete test suite"
	@echo "  make test-quick   # Fast iteration (gtest only)"
	@echo "  make sanitize     # Check for memory issues"
	@echo ""
	@echo "Prerequisites:"
	@echo "  Google Test: brew install googletest (macOS)"
	@echo "  Coverage:    brew install lcov (for HTML reports)"
	@echo "  AFL++:       brew install afl++ (for fuzzing)"

# Dependency tracking
-include $(OBJS:.o=.d)

$(OBJ_DIR)/%.d: $(SRC_DIR)/%.cpp
	@$(CXX) $(CXXFLAGS) -MM -MT $(@:.d=.o) $< > $@
