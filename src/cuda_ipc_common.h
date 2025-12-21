#pragma once

#include <cuda.h>
#include <cstddef>
#include <cstdio>
#include <cstdlib>

// CUDA error checking macro
#define CHECK_CUDA(call) checkCudaError((call), #call, __FILE__, __LINE__)

// Error handling
void checkCudaError(CUresult result, const char* call, const char* file, int line);

// CUDA initialization
CUdevice initCudaDevice(int device_id = 0);
CUcontext createCudaContext(CUdevice device);

// VMM support checking
bool checkVMMSupport(CUdevice device);

// Memory granularity
size_t getMemoryGranularity(CUdevice device);
size_t alignSize(size_t size, size_t granularity);

// Data transfer
void copyHostToDevice(CUdeviceptr dst, const void* src, size_t size);
void copyDeviceToHost(void* dst, CUdeviceptr src, size_t size);

// Test data generation and verification
void generateTestData(int* buffer, size_t count);
bool verifyTestData(const int* buffer, size_t count);
