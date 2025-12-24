#include "cuda_ro_internal.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>

WrapperState::WrapperState() : shm_fd_(-1), shared_handle_map_(nullptr) {
}

WrapperState::~WrapperState() {
    cleanupSharedMemory();
}

WrapperState& WrapperState::getInstance() {
    static WrapperState instance;
    return instance;
}

void WrapperState::initSharedMemory() {
    // Open/create shared memory object
    shm_fd_ = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd_ < 0) {
        log_error("Failed to open shared memory: %s", strerror(errno));
        return;
    }

    // Set size
    if (ftruncate(shm_fd_, sizeof(SharedHandleMap)) < 0) {
        log_error("Failed to resize shared memory");
        return;
    }

    // Map into address space
    shared_handle_map_ = (SharedHandleMap*)mmap(NULL, sizeof(SharedHandleMap),
        PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd_, 0);

    if (shared_handle_map_ == MAP_FAILED) {
        log_error("Failed to mmap shared memory");
        shared_handle_map_ = nullptr;
        return;
    }

    // Initialize on first use (check if handle_count is zero)
    int expected = 0;
    if (shared_handle_map_->handle_count.compare_exchange_strong(expected, 0)) {
        // First process to create the shared memory - initialize mutex
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
        pthread_mutex_init(&shared_handle_map_->lock, &attr);
        pthread_mutexattr_destroy(&attr);

        // Initialize all entries
        for (int i = 0; i < MAX_SHARED_HANDLES; i++) {
            shared_handle_map_->entries[i].dev = 0;
            shared_handle_map_->entries[i].ino = 0;
            shared_handle_map_->entries[i].is_readonly.store(false);
            shared_handle_map_->entries[i].owner_pid = 0;
        }
    }
}

void WrapperState::cleanupSharedMemory() {
    if (shared_handle_map_) {
        munmap(shared_handle_map_, sizeof(SharedHandleMap));
        shared_handle_map_ = nullptr;
    }
    if (shm_fd_ >= 0) {
        close(shm_fd_);
        shm_fd_ = -1;
    }
    // Note: Don't unlink shared memory here - other processes may still use it
    // shm_unlink(SHM_NAME);
}

void WrapperState::registerAllocation(CUmemGenericAllocationHandle handle, size_t size) {
    std::lock_guard<std::mutex> lock(mutex_);
    AllocationMetadata meta;
    meta.handle = handle;
    meta.size = size;
    meta.is_read_only = false;
    meta.exported_fd = -1;
    allocations_[handle] = meta;
}

void WrapperState::markAsReadOnly(CUmemGenericAllocationHandle handle) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = allocations_.find(handle);
    if (it != allocations_.end()) {
        it->second.is_read_only = true;
    }
}

void WrapperState::unregisterAllocation(CUmemGenericAllocationHandle handle) {
    std::lock_guard<std::mutex> lock(mutex_);
    allocations_.erase(handle);
}

bool WrapperState::isHandleReadOnly(CUmemGenericAllocationHandle handle) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = allocations_.find(handle);
    if (it != allocations_.end()) {
        return it->second.is_read_only;
    }
    return false;
}

void WrapperState::markFdAsReadOnly(int fd) {
    if (!shared_handle_map_) return;

    // Get dev/ino for this FD using fstat
    struct stat st;
    if (fstat(fd, &st) != 0) {
        log_error("fstat failed for FD %d: %s", fd, strerror(errno));
        return;
    }

    pthread_mutex_lock(&shared_handle_map_->lock);

    // Find if this dev/ino already exists
    int count = shared_handle_map_->handle_count.load();
    for (int i = 0; i < count && i < MAX_SHARED_HANDLES; i++) {
        if (shared_handle_map_->entries[i].dev == st.st_dev &&
            shared_handle_map_->entries[i].ino == st.st_ino) {
            shared_handle_map_->entries[i].is_readonly.store(true);
            pthread_mutex_unlock(&shared_handle_map_->lock);
            log_info("Marked dev=%llu ino=%llu as read-only (FD %d)",
                     (unsigned long long)st.st_dev, (unsigned long long)st.st_ino, fd);
            return;
        }
    }

    // Add new entry
    int idx = shared_handle_map_->handle_count.fetch_add(1);
    if (idx < MAX_SHARED_HANDLES) {
        shared_handle_map_->entries[idx].dev = st.st_dev;
        shared_handle_map_->entries[idx].ino = st.st_ino;
        shared_handle_map_->entries[idx].is_readonly.store(true);
        shared_handle_map_->entries[idx].owner_pid = getpid();
        log_info("Added dev=%llu ino=%llu as read-only (FD %d)",
                 (unsigned long long)st.st_dev, (unsigned long long)st.st_ino, fd);
    }

    pthread_mutex_unlock(&shared_handle_map_->lock);
}

bool WrapperState::isFdReadOnly(int fd) {
    if (!shared_handle_map_) return false;

    // Get dev/ino for this FD using fstat
    struct stat st;
    if (fstat(fd, &st) != 0) {
        log_error("fstat failed for FD %d: %s", fd, strerror(errno));
        return false;
    }

    pthread_mutex_lock(&shared_handle_map_->lock);

    bool result = false;
    int count = shared_handle_map_->handle_count.load();
    for (int i = 0; i < count && i < MAX_SHARED_HANDLES; i++) {
        if (shared_handle_map_->entries[i].dev == st.st_dev &&
            shared_handle_map_->entries[i].ino == st.st_ino) {
            result = shared_handle_map_->entries[i].is_readonly.load();
            log_info("Checked dev=%llu ino=%llu (FD %d): is_readonly=%d",
                     (unsigned long long)st.st_dev, (unsigned long long)st.st_ino, fd, result);
            break;
        }
    }

    pthread_mutex_unlock(&shared_handle_map_->lock);
    return result;
}

void WrapperState::registerMapping(CUdeviceptr ptr, CUmemGenericAllocationHandle handle, size_t size) {
    std::lock_guard<std::mutex> lock(mutex_);
    ptr_to_handle_[ptr] = handle;
}

void WrapperState::unregisterMapping(CUdeviceptr ptr) {
    std::lock_guard<std::mutex> lock(mutex_);
    ptr_to_handle_.erase(ptr);
}

bool WrapperState::isDevicePtrReadOnly(CUdeviceptr ptr) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = ptr_to_handle_.find(ptr);
    if (it != ptr_to_handle_.end()) {
        CUmemGenericAllocationHandle handle = it->second;
        auto alloc_it = allocations_.find(handle);
        if (alloc_it != allocations_.end()) {
            return alloc_it->second.is_read_only;
        }
    }
    return false;
}
