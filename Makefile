# Compiler and flags
CXX := g++
CXXFLAGS := -O3 -std=c++17 -fPIC -Wall -Iinc
LDFLAGS := -lpthread

SRC_DIR := src
OBJ_DIR := obj
LIB_DIR := lib
BIN_DIR := bin

HASH_SRC := $(SRC_DIR)/shielding_hash.cpp
ARRAY_SRC := $(SRC_DIR)/shielding_array.cpp
BENCH_SRC := $(SRC_DIR)/lock_bench.cpp

HASH_OBJ := $(OBJ_DIR)/shielding_hash.o
ARRAY_OBJ := $(OBJ_DIR)/shielding_array.o

HASH_SO := $(LIB_DIR)/libshielding_hash.so
ARRAY_SO := $(LIB_DIR)/libshielding_array.so

# Default lock type (override with make LOCK_DEF=TAS3)
LOCK_DEF ?= MCS3
LOCK_LOWER := $(shell echo $(LOCK_DEF) | tr A-Z a-z)
BENCH_BIN := $(BIN_DIR)/$(LOCK_LOWER)_lock_bench

.PHONY: all clean

all: $(HASH_SO) $(ARRAY_SO) $(BENCH_BIN)

$(OBJ_DIR) $(LIB_DIR) $(BIN_DIR):
	mkdir -p $@

$(HASH_OBJ): $(HASH_SRC) | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(ARRAY_OBJ): $(ARRAY_SRC) | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(HASH_SO): $(HASH_OBJ) | $(LIB_DIR)
	$(CXX) -shared -o $@ $<

$(ARRAY_SO): $(ARRAY_OBJ) | $(LIB_DIR)
	$(CXX) -shared -o $@ $<

# Detect required library based on LOCK_DEF
ifeq ($(findstring 3,$(LOCK_DEF)),3)
    EXTRA_LIB := -lshielding_array
else ifeq ($(findstring 4,$(LOCK_DEF)),4)
    EXTRA_LIB := -lshielding_hash
else
    EXTRA_LIB :=
endif

$(BENCH_BIN): $(BENCH_SRC) $(HASH_SO) $(ARRAY_SO) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -I$(SRC_DIR) -D$(LOCK_DEF) $(BENCH_SRC) \
	-L$(LIB_DIR) $(EXTRA_LIB) -Wl,-rpath=$(LIB_DIR) $(LDFLAGS) -o $@

clean:
	rm -rf $(OBJ_DIR) $(LIB_DIR) $(BIN_DIR)
