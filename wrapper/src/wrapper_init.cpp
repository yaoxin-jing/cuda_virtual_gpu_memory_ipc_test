#define _GNU_SOURCE
#include "cuda_ro_internal.h"
#include "cuda_real_funcs.h"
#include <dlfcn.h>
#include <cstdlib>
#include <cstdio>

// Global function pointers (definition)
RealCudaFunctions g_real_cuda;

// Constructor loads real symbols (runs when .so is loaded, before main())
__attribute__((constructor))
static void init_wrapper() {
    // Try to load libcuda.so.1 directly
    void* libcuda = dlopen("libcuda.so.1", RTLD_LAZY | RTLD_NOLOAD);
    if (!libcuda) {
        libcuda = dlopen("libcuda.so.1", RTLD_LAZY);
    }
    if (!libcuda) {
        fprintf(stderr, "ERROR: Failed to load libcuda.so.1: %s\n", dlerror());
        abort();
    }

    // Load CUDA VMM function pointers
    g_real_cuda.cuInit = (cuInit_t)dlsym(libcuda, "cuInit");
    g_real_cuda.cuMemCreate = (cuMemCreate_t)dlsym(libcuda, "cuMemCreate");
    g_real_cuda.cuMemRelease = (cuMemRelease_t)dlsym(libcuda, "cuMemRelease");
    g_real_cuda.cuMemMap = (cuMemMap_t)dlsym(libcuda, "cuMemMap");
    g_real_cuda.cuMemUnmap = (cuMemUnmap_t)dlsym(libcuda, "cuMemUnmap");
    g_real_cuda.cuMemSetAccess = (cuMemSetAccess_t)dlsym(libcuda, "cuMemSetAccess");
    g_real_cuda.cuMemExportToShareableHandle = (cuMemExportToShareableHandle_t)dlsym(libcuda, "cuMemExportToShareableHandle");
    g_real_cuda.cuMemImportFromShareableHandle = (cuMemImportFromShareableHandle_t)dlsym(libcuda, "cuMemImportFromShareableHandle");

    if (!g_real_cuda.cuInit || !g_real_cuda.cuMemCreate) {
        fprintf(stderr, "ERROR: Failed to load CUDA symbols\n");
        abort();
    }
}

// Intercept cuInit to initialize shared memory
extern "C" CUresult cuInit(unsigned int Flags) {
    // Call real CUDA init FIRST
    CUresult result = g_real_cuda.cuInit(Flags);

    // Initialize shared memory exactly once on CUDA initialization
    static bool initialized = false;
    if (!initialized && result == CUDA_SUCCESS) {
        WrapperState::getInstance().initSharedMemory();
        initialized = true;
    }

    return result;
}

// Destructor for cleanup
__attribute__((destructor))
static void cleanup_wrapper() {
    WrapperState::getInstance().cleanupSharedMemory();
}
