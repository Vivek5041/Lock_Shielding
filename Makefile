# Compiler and flags
CXX := g++
CXXFLAGS := -O2 -std=c++17 -fPIC -Wall -Iinc -fno-inline
LDFLAGS := -lpthread

SRC_DIR := src
OBJ_DIR := obj
LIB_DIR := lib
BIN_DIR := bin

HASH_SRC := $(SRC_DIR)/shielding_hash.cpp
ARRAY_SRC := $(SRC_DIR)/shielding_array.cpp
INVOKE_SRC := $(SRC_DIR)/shielding_invoke.cpp
BENCH_SRC := $(SRC_DIR)/lock_bench.cpp

HASH_OBJ := $(OBJ_DIR)/shielding_hash.o
ARRAY_OBJ := $(OBJ_DIR)/shielding_array.o
INVOKE_OBJ := $(OBJ_DIR)/shielding_invoke.o

HASH_SO := $(LIB_DIR)/libshielding_hash.so
ARRAY_SO := $(LIB_DIR)/libshielding_array.so
INVOKE_SO := $(LIB_DIR)/libshielding_invoke.so

# Default lock type (override with make LOCK_DEF=MCS_BASELINE)
LOCK_DEF ?= MCS_BASELINE
LOCK_LOWER := $(shell echo $(LOCK_DEF) | tr A-Z a-z)
BENCH_BIN := $(BIN_DIR)/$(LOCK_LOWER)_lock_bench

# Test configuration
TEST_DIR := tests
TEST_BINS := $(BIN_DIR)/test_array $(BIN_DIR)/test_hash $(BIN_DIR)/test_invoke $(BIN_DIR)/test_integration

.PHONY: all clean test test-array test-hash test-invoke test-integration test-all

all: inc/topology.h $(HASH_SO) $(ARRAY_SO) $(INVOKE_SO) $(BENCH_BIN)

inc/topology.h: inc/topology.in
	cat $< | sed -e "s/@nodes@/$$(numactl -H | head -1 | cut -f 2 -d' ')/g" > $@
	sed -i "s/@cpus@/$$(nproc)/g" $@
	sed -i "s/@cachelinesize@/128/g" $@  # 128 bytes is advised by intel documentation to avoid false-sharing with the HW prefetcher
	sed -i "s/@pagesize@/$$(getconf PAGESIZE)/g" $@
	sed -i 's#@cpufreq@#'$$(cat /proc/cpuinfo | grep MHz | head -1 | awk '{ x = $$4/1000; printf("%0.2g", x); }')'#g' $@
	chmod a+x $@


$(OBJ_DIR) $(LIB_DIR) $(BIN_DIR):
	mkdir -p $@

$(HASH_OBJ): $(HASH_SRC) | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(ARRAY_OBJ): $(ARRAY_SRC) | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(INVOKE_OBJ): $(INVOKE_SRC) | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(HASH_SO): $(HASH_OBJ) | $(LIB_DIR)
	$(CXX) -shared -o $@ $<

$(ARRAY_SO): $(ARRAY_OBJ) | $(LIB_DIR)
	$(CXX) -shared -o $@ $<

$(INVOKE_SO): $(INVOKE_OBJ) | $(LIB_DIR)
	$(CXX) -shared -o $@ $<

# Detect required library based on LOCK_DEF
ifeq ($(findstring LS_ARRAY,$(LOCK_DEF)),LS_ARRAY)
    EXTRA_LIB := -lshielding_array
else ifeq ($(findstring LS_HYBRID,$(LOCK_DEF)),LS_HYBRID)
    EXTRA_LIB := -lshielding_hash
else ifeq ($(findstring LS_INVOKE,$(LOCK_DEF)),LS_INVOKE)
    EXTRA_LIB := -lshielding_invoke    
else
    EXTRA_LIB :=
endif

$(BENCH_BIN): $(BENCH_SRC) $(HASH_SO) $(ARRAY_SO) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -I$(SRC_DIR) -D$(LOCK_DEF) $(BENCH_SRC) \
	-L$(LIB_DIR) $(EXTRA_LIB) -Wl,-rpath=$(LIB_DIR) $(LDFLAGS) -o $@

# Test targets
test: test-all

$(BIN_DIR)/test_array: $(TEST_DIR)/test_array_shielding.cpp $(ARRAY_SO) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -I$(SRC_DIR) $(TEST_DIR)/test_array_shielding.cpp \
	-L$(LIB_DIR) -lshielding_array -Wl,-rpath=$(LIB_DIR) $(LDFLAGS) -o $@

$(BIN_DIR)/test_hash: $(TEST_DIR)/test_hash_shielding.cpp $(HASH_SO) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -I$(SRC_DIR) $(TEST_DIR)/test_hash_shielding.cpp \
	-L$(LIB_DIR) -lshielding_hash -Wl,-rpath=$(LIB_DIR) $(LDFLAGS) -o $@

$(BIN_DIR)/test_invoke: $(TEST_DIR)/test_invoke_shielding.cpp $(INVOKE_SO) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -I$(SRC_DIR) $(TEST_DIR)/test_invoke_shielding.cpp \
	-L$(LIB_DIR) -lshielding_invoke -Wl,-rpath=$(LIB_DIR) $(LDFLAGS) -o $@

$(BIN_DIR)/test_integration: $(TEST_DIR)/test_integration.cpp $(HASH_SO) $(ARRAY_SO) $(INVOKE_SO) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -I$(SRC_DIR) -DMCS_LS_ARRAY $(TEST_DIR)/test_integration.cpp \
	-L$(LIB_DIR) -lshielding_array -Wl,-rpath=$(LIB_DIR) $(LDFLAGS) -o $@

test-array: $(BIN_DIR)/test_array
	@echo "Running Array Shielding Tests..."
	@$(BIN_DIR)/test_array

test-hash: $(BIN_DIR)/test_hash
	@echo "Running Hash Shielding Tests..."
	@$(BIN_DIR)/test_hash

test-invoke: $(BIN_DIR)/test_invoke
	@echo "Running Invoke Shielding Tests..."
	@$(BIN_DIR)/test_invoke

test-integration: $(BIN_DIR)/test_integration
	@echo "Running Integration Tests..."
	@$(BIN_DIR)/test_integration

test-all: test-array test-hash test-invoke test-integration
	@echo "All tests completed!"

# Test with different lock types and shielding combinations
test-combinations:
	@echo "Testing various lock/shielding combinations..."
	@for combo in MCS_BASELINE CLH_LS_ARRAY TAS_LS_HYBRID TICKET_LS_INVOKE; do \
		echo "Testing $$combo..."; \
		$(MAKE) clean > /dev/null 2>&1; \
		$(MAKE) LOCK_DEF=$$combo $(BIN_DIR)/test_integration > /dev/null 2>&1 && \
		$(BIN_DIR)/test_integration > /dev/null 2>&1 && \
		echo "✓ $$combo: PASSED" || echo "✗ $$combo: FAILED"; \
	done

clean:
	rm -rf $(OBJ_DIR) $(LIB_DIR) $(BIN_DIR) inc/topology.h
