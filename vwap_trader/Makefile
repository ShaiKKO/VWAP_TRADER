# VWAP Trading System Makefile

# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++14 -Wall -Wextra -pedantic
INCLUDES = -I./include
LDFLAGS = -pthread

# Build directories
SRCDIR = src
INCDIR = include
TESTDIR = tests
OBJDIR = obj
BINDIR = bin

# Target executables
TARGET = $(BINDIR)/vwap_trader
SIMULATOR = $(BINDIR)/market_simulator
BENCHMARK = $(BINDIR)/benchmark

# Source files
SOURCES = $(wildcard $(SRCDIR)/*.cpp)
# Exclude simulator_main.cpp, benchmark.cpp, and optimized files from main build
MAIN_SOURCES = $(filter-out $(SRCDIR)/simulator_main.cpp $(SRCDIR)/benchmark.cpp $(SRCDIR)/%_optimized.cpp,$(SOURCES))
MAIN_OBJECTS = $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(MAIN_SOURCES))

# Simulator sources (exclude main.cpp, benchmark.cpp, and optimized files)
SIM_SOURCES = $(filter-out $(SRCDIR)/main.cpp $(SRCDIR)/benchmark.cpp $(SRCDIR)/%_optimized.cpp,$(SOURCES))
SIM_OBJECTS = $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SIM_SOURCES))

# Test files
TEST_SOURCES = $(wildcard $(TESTDIR)/*.cpp)
TEST_OBJECTS = $(patsubst $(TESTDIR)/%.cpp,$(OBJDIR)/test_%.o,$(TEST_SOURCES))

# Default target
all: release simulator benchmark

# Release build with optimization
release: CXXFLAGS += -O2 -DNDEBUG
release: $(TARGET)

# Debug build with symbols
debug: CXXFLAGS += -g -O0 -DDEBUG
debug: $(TARGET) $(SIMULATOR)

# Build simulator
simulator: CXXFLAGS += -O2 -DNDEBUG
simulator: $(SIMULATOR)

# Build benchmark
benchmark: CXXFLAGS += -O2 -DNDEBUG
benchmark: $(BENCHMARK)

# Create directories if they don't exist
$(OBJDIR):
	@mkdir -p $(OBJDIR)

$(BINDIR):
	@mkdir -p $(BINDIR)

# Build target executable
$(TARGET): $(BINDIR) $(OBJDIR) $(MAIN_OBJECTS)
	@echo "Linking $@..."
	@$(CXX) $(MAIN_OBJECTS) -o $@ $(LDFLAGS)
	@echo "Build complete: $@"

# Build simulator executable
$(SIMULATOR): $(BINDIR) $(OBJDIR) $(SIM_OBJECTS)
	@echo "Linking $@..."
	@$(CXX) $(SIM_OBJECTS) -o $@ $(LDFLAGS)
	@echo "Build complete: $@"

# Build benchmark executable  
$(BENCHMARK): $(BINDIR) $(OBJDIR) $(filter-out $(OBJDIR)/main.o $(OBJDIR)/simulator_main.o,$(MAIN_OBJECTS)) $(OBJDIR)/benchmark.o
	@echo "Linking $@..."
	@$(CXX) $(filter-out $(OBJDIR)/main.o $(OBJDIR)/simulator_main.o,$(MAIN_OBJECTS)) $(OBJDIR)/benchmark.o -o $@ $(LDFLAGS)
	@echo "Build complete: $@"

# Compile source files
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp $(OBJDIR)
	@echo "Compiling $<..."
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Test files for main test runner
TEST_MAIN = $(TESTDIR)/test_main.cpp
TEST_PROTOCOL = $(TESTDIR)/test_protocol.cpp
TEST_VWAP = $(TESTDIR)/test_vwap.cpp

# Test target
test: CXXFLAGS += -g -O0
test: $(OBJDIR) $(BINDIR) $(filter-out $(OBJDIR)/main.o $(OBJDIR)/simulator_main.o,$(MAIN_OBJECTS))
	@echo "Building tests..."
	@$(CXX) $(CXXFLAGS) $(INCLUDES) $(TEST_MAIN) \
		$(filter-out $(OBJDIR)/main.o $(OBJDIR)/simulator_main.o,$(MAIN_OBJECTS)) -o $(BINDIR)/test_runner $(LDFLAGS)
	@echo "Running tests..."
	@$(BINDIR)/test_runner
	@echo "Tests complete"

# Comprehensive test suite
COMPREHENSIVE_TEST_SOURCES = $(TESTDIR)/test_comprehensive_runner.cpp \
	$(TESTDIR)/test_vwap_accuracy.cpp \
	$(TESTDIR)/test_order_triggering.cpp \
	$(TESTDIR)/test_order_size.cpp \
	$(TESTDIR)/test_window_timing.cpp \
	$(TESTDIR)/test_network_resilience.cpp \
	$(TESTDIR)/test_binary_protocol.cpp \
	$(TESTDIR)/test_edge_cases.cpp \
	$(TESTDIR)/test_performance.cpp \
	$(TESTDIR)/test_stress.cpp

test-comprehensive: CXXFLAGS += -O2 -DNDEBUG
test-comprehensive: $(OBJDIR) $(BINDIR) $(filter-out $(OBJDIR)/main.o $(OBJDIR)/simulator_main.o $(OBJDIR)/benchmark.o,$(MAIN_OBJECTS))
	@echo "Building comprehensive test suite..."
	@$(CXX) $(CXXFLAGS) $(INCLUDES) $(COMPREHENSIVE_TEST_SOURCES) \
		$(filter-out $(OBJDIR)/main.o $(OBJDIR)/simulator_main.o $(OBJDIR)/benchmark.o,$(MAIN_OBJECTS)) \
		-o $(BINDIR)/test_comprehensive $(LDFLAGS)
	@echo "Running comprehensive tests..."
	@$(BINDIR)/test_comprehensive
	@echo "Comprehensive tests complete"

# Compile test files
$(OBJDIR)/test_%.o: $(TESTDIR)/%.cpp $(OBJDIR)
	@echo "Compiling test $<..."
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Clean build artifacts
clean:
	@echo "Cleaning..."
	@rm -rf $(OBJDIR) $(BINDIR)
	@echo "Clean complete"

# Run the program with example arguments
run: release
	@echo "Running VWAP trader..."
	$(TARGET) IBM B 100 30 127.0.0.1 14000 127.0.0.1 15000

# Run the simulator
run-simulator: simulator
	@echo "Running Market Data Simulator..."
	$(SIMULATOR) --verbose --rate 10 --duration 60

# Run benchmark
run-benchmark: benchmark
	@echo "Running Performance Benchmark..."
	$(BENCHMARK)

# Check code style (requires cppcheck)
check:
	@echo "Running static analysis..."
	@which cppcheck > /dev/null 2>&1 && cppcheck --enable=all --suppress=missingIncludeSystem $(SRCDIR) $(INCDIR) || echo "cppcheck not found, skipping..."

# Generate documentation (requires doxygen)
docs:
	@echo "Generating documentation..."
	@which doxygen > /dev/null 2>&1 && doxygen Doxyfile || echo "doxygen not found, skipping..."

# Show help
help:
	@echo "VWAP Trading System - Makefile targets:"
	@echo "  make all               - Build release version and simulator"
	@echo "  make release           - Build optimized release version"
	@echo "  make debug             - Build debug version with symbols"
	@echo "  make simulator         - Build market data simulator"
	@echo "  make benchmark         - Build performance benchmark"
	@echo "  make test              - Build and run basic tests"
	@echo "  make test-comprehensive - Build and run comprehensive test suite"
	@echo "  make clean             - Remove all build artifacts"
	@echo "  make run               - Run VWAP trader with example arguments"
	@echo "  make run-simulator     - Run market simulator"
	@echo "  make run-benchmark     - Run performance benchmark"
	@echo "  make check             - Run static code analysis"
	@echo "  make docs              - Generate documentation"
	@echo "  make help              - Show this help message"

# Phony targets
.PHONY: all release debug simulator test clean run run-simulator check docs help

# Dependencies
-include $(OBJECTS:.o=.d)
-include $(TEST_OBJECTS:.o=.d)