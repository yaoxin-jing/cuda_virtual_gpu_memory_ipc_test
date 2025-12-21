#include "cuda_ipc_common.h"
#include <cstring>

void checkCudaError(CUresult result, const char* call, const char* file, int line) {
    if (result != CUDA_SUCCESS) {
        const char* error_str;
        cuGetErrorString(result, &error_str);
        fprintf(stderr, "CUDA error at %s:%d\n", file, line);
        fprintf(stderr, "  %s failed with: %s\n", call, error_str);
        exit(1);
    }
}

CUdevice initCudaDevice(int device_id) {
    CHECK_CUDA(cuInit(0));

    CUdevice device;
    CHECK_CUDA(cuDeviceGet(&device, device_id));

    char name[256];
    CHECK_CUDA(cuDeviceGetName(name, sizeof(name), device));
    printf("Using CUDA device %d: %s\n", device_id, name);

    return device;
}

CUcontext createCudaContext(CUdevice device) {
    CUcontext context;
    // Use primary context instead of cuCtxCreate for simplicity
    CHECK_CUDA(cuDevicePrimaryCtxRetain(&context, device));
    CHECK_CUDA(cuCtxSetCurrent(context));
    return context;
}

bool checkVMMSupport(CUdevice device) {
    int vmm_supported = 0;
    CHECK_CUDA(cuDeviceGetAttribute(&vmm_supported,
        CU_DEVICE_ATTRIBUTE_VIRTUAL_MEMORY_MANAGEMENT_SUPPORTED, device));

    if (!vmm_supported) {
        fprintf(stderr, "Error: Virtual Memory Management not supported on this device\n");
        fprintf(stderr, "Requires GPU with compute capability 6.0+ (Pascal or newer)\n");
        return false;
    }

    printf("VMM support: yes\n");
    return true;
}

size_t getMemoryGranularity(CUdevice device) {
    CUmemAllocationProp prop = {};
    prop.type = CU_MEM_ALLOCATION_TYPE_PINNED;
    prop.location.type = CU_MEM_LOCATION_TYPE_DEVICE;
    prop.location.id = device;
    prop.requestedHandleTypes = CU_MEM_HANDLE_TYPE_POSIX_FILE_DESCRIPTOR;

    size_t granularity;
    CHECK_CUDA(cuMemGetAllocationGranularity(&granularity, &prop,
        CU_MEM_ALLOC_GRANULARITY_MINIMUM));

    printf("Memory granularity: %zu bytes\n", granularity);
    return granularity;
}

size_t alignSize(size_t size, size_t granularity) {
    return ((size + granularity - 1) / granularity) * granularity;
}

void copyHostToDevice(CUdeviceptr dst, const void* src, size_t size) {
    CHECK_CUDA(cuMemcpyHtoD(dst, src, size));
}

void copyDeviceToHost(void* dst, CUdeviceptr src, size_t size) {
    CHECK_CUDA(cuMemcpyDtoH(dst, src, size));
}

void generateTestData(int* buffer, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        buffer[i] = (i * 2 + 1337) ^ 0xDEADBEEF;
    }
}

bool verifyTestData(const int* buffer, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        int expected = (i * 2 + 1337) ^ 0xDEADBEEF;
        if (buffer[i] != expected) {
            fprintf(stderr, "Data mismatch at index %zu: expected %d, got %d\n",
                    i, expected, buffer[i]);
            return false;
        }
    }
    return true;
}
