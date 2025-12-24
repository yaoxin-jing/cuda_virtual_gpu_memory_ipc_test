#include "cuda_ro_internal.h"
#include "cuda_real_funcs.h"

extern "C" CUresult cuMemCreate(CUmemGenericAllocationHandle* handle,
                                 size_t size,
                                 const CUmemAllocationProp* prop,
                                 unsigned long long flags) {
    // Call real function
    CUresult result = g_real_cuda.cuMemCreate(handle, size, prop, flags);

    if (result == CUDA_SUCCESS) {
        // Register allocation (initially read-write)
        WrapperState::getInstance().registerAllocation(*handle, size);
    }

    return result;
}

extern "C" CUresult cuMemRelease(CUmemGenericAllocationHandle handle) {
    WrapperState::getInstance().unregisterAllocation(handle);
    return g_real_cuda.cuMemRelease(handle);
}

extern "C" CUresult cuMemMap(CUdeviceptr ptr, size_t size, size_t offset,
                              CUmemGenericAllocationHandle handle,
                              unsigned long long flags) {
    CUresult result = g_real_cuda.cuMemMap(ptr, size, offset, handle, flags);

    if (result == CUDA_SUCCESS) {
        // Track ptr -> handle mapping for runtime checks
        WrapperState::getInstance().registerMapping(ptr, handle, size);
    }

    return result;
}

extern "C" CUresult cuMemUnmap(CUdeviceptr ptr, size_t size) {
    WrapperState::getInstance().unregisterMapping(ptr);
    return g_real_cuda.cuMemUnmap(ptr, size);
}
