# CUDA paths
CUDA_PATH ?= /usr/local/cuda
CUDA_INC = $(CUDA_PATH)/include
CUDA_LIB = $(CUDA_PATH)/lib64

# Compiler flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -I$(CUDA_INC) -I$(WRAPPER_INC_DIR)
LDFLAGS = -L$(CUDA_LIB) -lcuda

# Directories
SRC_DIR = src
BUILD_DIR = build
WRAPPER_DIR = wrapper
WRAPPER_SRC_DIR = $(WRAPPER_DIR)/src
WRAPPER_INC_DIR = $(WRAPPER_DIR)/include

# Source files
COMMON_SRC = $(SRC_DIR)/cuda_ipc_common.cpp $(SRC_DIR)/ipc_socket.cpp
PRODUCER_SRC = $(SRC_DIR)/producer.cpp
CONSUMER_SRC = $(SRC_DIR)/consumer.cpp

# Wrapper sources
WRAPPER_SRCS = $(wildcard $(WRAPPER_SRC_DIR)/*.cpp)
WRAPPER_OBJS = $(WRAPPER_SRCS:$(WRAPPER_SRC_DIR)/%.cpp=$(BUILD_DIR)/wrapper_%.o)

# Targets
PRODUCER = $(BUILD_DIR)/producer
CONSUMER = $(BUILD_DIR)/consumer
WRAPPER_LIB = $(BUILD_DIR)/libcuda_ro_wrapper.so

.PHONY: all clean test wrapper

all: $(BUILD_DIR) $(PRODUCER) $(CONSUMER) $(WRAPPER_LIB)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(PRODUCER): $(PRODUCER_SRC) $(COMMON_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(CONSUMER): $(CONSUMER_SRC) $(COMMON_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Wrapper library build
$(BUILD_DIR)/wrapper_%.o: $(WRAPPER_SRC_DIR)/%.cpp
	$(CXX) -std=c++17 -Wall -Wextra -fPIC -I$(CUDA_INC) -I$(WRAPPER_INC_DIR) -c -o $@ $<

$(WRAPPER_LIB): $(WRAPPER_OBJS)
	$(CXX) -shared -ldl -lpthread -lrt -o $@ $^

wrapper: $(WRAPPER_LIB)

clean:
	rm -rf $(BUILD_DIR)
	rm -f /tmp/cuda_vmm_test.sock
	rm -f /dev/shm/cuda_ro_wrapper_fds

test: all
	@echo "Starting producer in background..."
	@$(PRODUCER) & PID=$$!; sleep 2; $(CONSUMER); kill $$PID 2>/dev/null || true

test-wrapper: all
	@if ! command -v nvidia-smi >/dev/null 2>&1; then \
		echo "Skipping test-wrapper: nvidia-smi not found (CUDA driver likely absent)"; \
	elif ! nvidia-smi >/dev/null 2>&1; then \
		echo "Skipping test-wrapper: CUDA driver not available"; \
	else \
		echo "Testing with read-only wrapper..."; \
		LD_PRELOAD=$(WRAPPER_LIB) $(PRODUCER) & PID=$$!; sleep 2; \
		LD_PRELOAD=$(WRAPPER_LIB) $(CONSUMER); kill $$PID 2>/dev/null || true; \
	fi
