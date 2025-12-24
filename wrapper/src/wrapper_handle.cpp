#include "cuda_ro_internal.h"
#include "cuda_ro_wrapper.h"
#include "cuda_real_funcs.h"

extern "C" CUresult cuMemExportToShareableHandle(
    void* shareableHandle,
    CUmemGenericAllocationHandle handle,
    CUmemAllocationHandleType handleType,
    unsigned long long flags) {

    // Check if read-only flag is set
    bool is_readonly = (flags & CU_MEM_EXPORT_FLAGS_READONLY) != 0;

    // Remove our custom flag before calling real function
    unsigned long long real_flags = flags & ~CU_MEM_EXPORT_FLAGS_READONLY;

    // Call real CUDA function
    CUresult result = g_real_cuda.cuMemExportToShareableHandle(shareableHandle, handle, handleType, real_flags);

    if (result == CUDA_SUCCESS && is_readonly) {
        // Mark handle as read-only in process-local state
        WrapperState::getInstance().markAsReadOnly(handle);

        // Mark FD's (dev, ino) as read-only in shared memory
        // Dev/ino is invariant across FD passing via SCM_RIGHTS
        if (handleType == CU_MEM_HANDLE_TYPE_POSIX_FILE_DESCRIPTOR) {
            int fd = *(int*)shareableHandle;
            WrapperState::getInstance().markFdAsReadOnly(fd);
            log_info("Exported handle 0x%llx as read-only FD %d",
                     (unsigned long long)handle, fd);
        }
    }

    return result;
}

extern "C" CUresult cuMemImportFromShareableHandle(
    CUmemGenericAllocationHandle* handle,
    void* osHandle,
    CUmemAllocationHandleType shHandleType) {

    // Check if FD is marked as read-only (for POSIX FD type)
    bool is_readonly = false;
    if (shHandleType == CU_MEM_HANDLE_TYPE_POSIX_FILE_DESCRIPTOR) {
        int fd = (int)(intptr_t)osHandle;
        is_readonly = WrapperState::getInstance().isFdReadOnly(fd);
    }

    // Call real CUDA function
    CUresult result = g_real_cuda.cuMemImportFromShareableHandle(handle, osHandle, shHandleType);

    if (result == CUDA_SUCCESS) {
        // Register the new handle (size unknown at import time, set to 0)
        WrapperState::getInstance().registerAllocation(*handle, 0);

        // Propagate read-only status if FD was read-only
        if (is_readonly) {
            WrapperState::getInstance().markAsReadOnly(*handle);
            log_info("Imported handle 0x%llx as read-only",
                     (unsigned long long)*handle);
        }
    }

    return result;
}
