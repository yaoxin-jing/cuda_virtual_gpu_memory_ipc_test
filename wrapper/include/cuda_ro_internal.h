#ifndef CUDA_RO_INTERNAL_H
#define CUDA_RO_INTERNAL_H

#include <cuda.h>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <pthread.h>
#include <sys/types.h>

// Metadata for each allocation handle (process-local)
struct AllocationMetadata {
    CUmemGenericAllocationHandle handle;
    size_t size;
    bool is_read_only;
    int exported_fd;  // FD if exported, -1 otherwise
};

// Shared memory structure for cross-process tracking
// Track by (dev, ino) which is invariant across FD passing via SCM_RIGHTS
#define MAX_SHARED_HANDLES 1024

struct SharedHandleMap {
    std::atomic<int> handle_count;
    struct HandleEntry {
        dev_t dev;     // Device ID from fstat (identifies filesystem)
        ino_t ino;     // Inode number from fstat (unique within filesystem)
        std::atomic<bool> is_readonly;
        pid_t owner_pid;
    } entries[MAX_SHARED_HANDLES];
    pthread_mutex_t lock;
};

// Thread-safe global state singleton
class WrapperState {
public:
    static WrapperState& getInstance();

    // Initialize/cleanup shared memory
    void initSharedMemory();
    void cleanupSharedMemory();

    // Allocation tracking (process-local)
    void registerAllocation(CUmemGenericAllocationHandle handle, size_t size);
    void markAsReadOnly(CUmemGenericAllocationHandle handle);
    void unregisterAllocation(CUmemGenericAllocationHandle handle);
    bool isHandleReadOnly(CUmemGenericAllocationHandle handle);

    // FD tracking (cross-process via shared memory using dev/ino)
    void markFdAsReadOnly(int fd);
    bool isFdReadOnly(int fd);

    // Device pointer tracking (for runtime checks)
    void registerMapping(CUdeviceptr ptr, CUmemGenericAllocationHandle handle, size_t size);
    void unregisterMapping(CUdeviceptr ptr);
    bool isDevicePtrReadOnly(CUdeviceptr ptr);

private:
    WrapperState();
    ~WrapperState();
    WrapperState(const WrapperState&) = delete;
    WrapperState& operator=(const WrapperState&) = delete;

    // Process-local state
    std::mutex mutex_;
    std::unordered_map<CUmemGenericAllocationHandle, AllocationMetadata> allocations_;
    std::unordered_map<CUdeviceptr, CUmemGenericAllocationHandle> ptr_to_handle_;

    // Shared memory for cross-process tracking
    int shm_fd_;
    SharedHandleMap* shared_handle_map_;
    static constexpr const char* SHM_NAME = "/cuda_ro_wrapper_handles";
};

// Logging utilities
void log_info(const char* format, ...);
void log_error(const char* format, ...);

#endif // CUDA_RO_INTERNAL_H
