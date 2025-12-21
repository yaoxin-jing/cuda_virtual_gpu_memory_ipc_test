# CUDA paths
CUDA_PATH ?= /usr/local/cuda
CUDA_INC = $(CUDA_PATH)/include
CUDA_LIB = $(CUDA_PATH)/lib64

# Compiler flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -I$(CUDA_INC)
LDFLAGS = -L$(CUDA_LIB) -lcuda

# Directories
SRC_DIR = src
BUILD_DIR = build

# Source files
COMMON_SRC = $(SRC_DIR)/cuda_ipc_common.cpp $(SRC_DIR)/ipc_socket.cpp
PRODUCER_SRC = $(SRC_DIR)/producer.cpp
CONSUMER_SRC = $(SRC_DIR)/consumer.cpp

# Targets
PRODUCER = $(BUILD_DIR)/producer
CONSUMER = $(BUILD_DIR)/consumer

.PHONY: all clean test

all: $(BUILD_DIR) $(PRODUCER) $(CONSUMER)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(PRODUCER): $(PRODUCER_SRC) $(COMMON_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(CONSUMER): $(CONSUMER_SRC) $(COMMON_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -rf $(BUILD_DIR)
	rm -f /tmp/cuda_vmm_test.sock

test: all
	@echo "Starting producer in background..."
	@$(PRODUCER) & PID=$$!; sleep 2; $(CONSUMER); kill $$PID 2>/dev/null || true