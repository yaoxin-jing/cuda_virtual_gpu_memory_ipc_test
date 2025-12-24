#include "cuda_ro_internal.h"
#include "cuda_real_funcs.h"

extern "C" CUresult cuMemSetAccess(CUdeviceptr ptr, size_t size,
                                    const CUmemAccessDesc* desc,
                                    size_t count) {
    // Check if any mapped region overlaps with read-only memory
    bool is_readonly = WrapperState::getInstance().isDevicePtrReadOnly(ptr);

    log_info("cuMemSetAccess: ptr=0x%llx, is_readonly=%d, flags=0x%x",
             (unsigned long long)ptr, is_readonly, count > 0 ? desc[0].flags : 0);

    if (is_readonly) {
        // Check if write permissions are requested
        // READWRITE = 0x3, READ = 0x1, so check if flags == READWRITE
        for (size_t i = 0; i < count; i++) {
            if (desc[i].flags == CU_MEM_ACCESS_FLAGS_PROT_READWRITE) {
                log_error("Rejected READWRITE access for read-only allocation at 0x%llx",
                         (unsigned long long)ptr);
                log_error("Consumer must use CU_MEM_ACCESS_FLAGS_PROT_READ instead");
                return CUDA_ERROR_INVALID_VALUE;
            }
        }
    }

    // Call real function (allows READ-only mappings)
    return g_real_cuda.cuMemSetAccess(ptr, size, desc, count);
}
