#ifndef CUDA_REAL_FUNCS_H
#define CUDA_REAL_FUNCS_H

#include <cuda.h>

// Function pointer types for CUDA VMM functions
typedef CUresult (*cuInit_t)(unsigned int);
typedef CUresult (*cuMemCreate_t)(CUmemGenericAllocationHandle*, size_t, const CUmemAllocationProp*, unsigned long long);
typedef CUresult (*cuMemRelease_t)(CUmemGenericAllocationHandle);
typedef CUresult (*cuMemMap_t)(CUdeviceptr, size_t, size_t, CUmemGenericAllocationHandle, unsigned long long);
typedef CUresult (*cuMemUnmap_t)(CUdeviceptr, size_t);
typedef CUresult (*cuMemSetAccess_t)(CUdeviceptr, size_t, const CUmemAccessDesc*, size_t);
typedef CUresult (*cuMemExportToShareableHandle_t)(void*, CUmemGenericAllocationHandle, CUmemAllocationHandleType, unsigned long long);
typedef CUresult (*cuMemImportFromShareableHandle_t)(CUmemGenericAllocationHandle*, void*, CUmemAllocationHandleType);

// Global function pointers structure
struct RealCudaFunctions {
    cuInit_t cuInit;
    cuMemCreate_t cuMemCreate;
    cuMemRelease_t cuMemRelease;
    cuMemMap_t cuMemMap;
    cuMemUnmap_t cuMemUnmap;
    cuMemSetAccess_t cuMemSetAccess;
    cuMemExportToShareableHandle_t cuMemExportToShareableHandle;
    cuMemImportFromShareableHandle_t cuMemImportFromShareableHandle;
};

// External reference to global structure (defined in wrapper_init.cpp)
extern RealCudaFunctions g_real_cuda;

#endif // CUDA_REAL_FUNCS_H
