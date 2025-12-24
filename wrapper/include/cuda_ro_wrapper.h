#ifndef CUDA_RO_WRAPPER_H
#define CUDA_RO_WRAPPER_H

#include <cuda.h>

// Custom read-only export flag (use bit 63 to avoid conflicts with CUDA flags)
#define CU_MEM_EXPORT_FLAGS_READONLY (1ULL << 63)

#endif // CUDA_RO_WRAPPER_H