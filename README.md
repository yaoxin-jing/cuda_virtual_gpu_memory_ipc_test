# CUDA Virtual Memory Mapping Test

A standalone educational application demonstrating GPU memory sharing between processes using CUDA Virtual Memory Management (VMM) API and Unix domain sockets with file descriptor passing (SCM_RIGHTS).

## Overview

This project implements a producer-consumer pattern where:
- **Producer**: Creates a GPU memory allocation, writes test data, exports it as a file descriptor, and sends the FD to the consumer via Unix socket
- **Consumer**: Receives the file descriptor, imports and maps the GPU memory, reads and verifies the data

## Key Technologies

- **CUDA Virtual Memory Management API**: Modern GPU memory management with explicit control
  - `cuMemCreate` - Create physical GPU memory allocation
  - `cuMemAddressReserve` - Reserve virtual address space
  - `cuMemMap` - Map physical memory to virtual address
  - `cuMemSetAccess` - Set read/write permissions
  - `cuMemExportToShareableHandle` - Export as POSIX file descriptor
  - `cuMemImportFromShareableHandle` - Import from file descriptor

- **Unix Domain Sockets with SCM_RIGHTS**: File descriptor passing between processes
  - `sendmsg()`/`recvmsg()` with control messages
  - `CMSG_*` macros for ancillary data

## Requirements

### Hardware
- NVIDIA GPU with compute capability 6.0+ (Pascal or newer)
- Same GPU accessible to both producer and consumer processes

### Software
- Linux kernel 2.6+ (for Unix domain sockets with SCM_RIGHTS)
- CUDA Toolkit 10.2+ (preferably 11.0+)
- gcc/g++ with C++17 support
- Recent NVIDIA driver with VMM support

## Build Instructions

```bash
cd /home/yj2124/cuda_mapping_test
make clean
make all
```

## Usage

### Option 1: Manual (Two Terminals)

Terminal 1 - Producer:
```bash
./build/producer
```

Terminal 2 - Consumer:
```bash
./build/consumer
```

### Option 2: Automated Test

```bash
make test
```

## Expected Output

### Producer
```
=== CUDA VMM Producer ===
Using CUDA device 0: <GPU name>
VMM support: yes
Memory granularity: <size> bytes
Buffer size: 1048576 bytes, aligned size: <aligned> bytes
Created physical memory allocation
Reserved virtual address space at 0x<address>
Mapped physical memory to virtual address
Set read/write access permissions
Generated 262144 test integers
Copied test data to GPU
Exported allocation as FD: <fd>
Waiting for consumer connection...
Consumer connected
Sent FD and size metadata to consumer
Consumer verified data successfully!
Cleanup complete
```

### Consumer
```
=== CUDA VMM Consumer ===
Using CUDA device 0: <GPU name>
Connecting to producer...
Connected to producer
Received FD: <fd>, size: <size> bytes
Imported allocation handle from FD
Reserved virtual address space at 0x<address>
Mapped imported memory to virtual address
Set read/write access permissions
Copied 1048576 bytes from GPU to host
Data verification PASSED (262144 integers verified)
Sent acknowledgment to producer
Cleanup complete
```

## How It Works

1. **Producer allocates GPU memory**:
   - Queries memory granularity requirements
   - Creates physical allocation with `cuMemCreate`
   - Reserves virtual address space with `cuMemAddressReserve`
   - Maps physical to virtual with `cuMemMap`
   - Sets permissions with `cuMemSetAccess`

2. **Producer writes test data**:
   - Generates verifiable pattern: `data[i] = (i * 2 + 1337) ^ 0xDEADBEEF`
   - Copies from host to GPU with `cuMemcpyHtoD`

3. **Producer exports and sends**:
   - Exports allocation as POSIX file descriptor with `cuMemExportToShareableHandle`
   - Sends FD via Unix socket using SCM_RIGHTS
   - Sends size metadata

4. **Consumer receives and imports**:
   - Receives FD via Unix socket SCM_RIGHTS
   - Imports handle with `cuMemImportFromShareableHandle`
   - Maps memory in consumer's address space

5. **Consumer reads and verifies**:
   - Copies data from GPU to host
   - Verifies against expected pattern
   - Sends ACK to producer

6. **Both processes cleanup**:
   - Unmap memory, free addresses, release handles
   - Close file descriptors

## Troubleshooting

| Error | Cause | Solution |
|-------|-------|----------|
| CUDA_ERROR_NOT_SUPPORTED | VMM not supported | Check GPU compute capability ≥6.0, CUDA ≥10.2 |
| CUDA_ERROR_INVALID_VALUE | Size not aligned | Automatically handled by alignSize() |
| Connection refused | Producer not running | Start producer first |
| Socket file exists | Previous run didn't clean up | Run `make clean` |
| Segfault on close | Wrong cleanup function | Consumer uses cuMemRelease, not cuMemFree |

## Learning Objectives

After running this application, you'll understand:
1. CUDA Driver API initialization and context management
2. Virtual Memory Management API (VMM) for fine-grained GPU memory control
3. File descriptor passing between processes using SCM_RIGHTS
4. Difference between physical allocation and virtual mapping
5. Process synchronization patterns
6. Proper cleanup sequences for shared GPU memory

## File Structure

```
.
├── Makefile                  # Build configuration
├── README.md                 # This file
└── src/
    ├── cuda_ipc_common.h    # CUDA utilities interface
    ├── cuda_ipc_common.cpp  # CUDA implementation
    ├── ipc_socket.h         # Socket interface
    ├── ipc_socket.cpp       # Socket implementation with SCM_RIGHTS
    ├── producer.cpp         # Producer process
    └── consumer.cpp         # Consumer process
```

## Advantages Over Legacy cuIpcGetMemHandle

1. **Explicit FD control**: Can pass file descriptors through any IPC mechanism
2. **Fine-grained mapping**: Map subranges of allocations using offset parameter
3. **Multiple mappings**: Same physical memory at different virtual addresses
4. **Cross-process flexibility**: Not limited to same executable or GPU pairing
5. **Better debugging**: FD lifecycle visible with standard tools (lsof, /proc/fd)

## References

- CUDA Driver API: https://docs.nvidia.com/cuda/cuda-driver-api/
- Virtual Memory Management: CUDA 10.2+ documentation
- Unix Domain Sockets: man sendmsg(2), man cmsg(3)