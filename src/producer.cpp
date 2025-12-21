#include "cuda_ipc_common.h"
#include "ipc_socket.h"
#include <vector>
#include <unistd.h>

int main() {
    printf("=== CUDA VMM Producer ===\n");

    // 1. Initialize CUDA
    CUdevice device = initCudaDevice(0);
    CUcontext context = createCudaContext(device);

    // 2. Check VMM support
    if (!checkVMMSupport(device)) {
        return 1;
    }

    // 3. Get memory granularity
    size_t granularity = getMemoryGranularity(device);

    // 4. Setup allocation properties
    CUmemAllocationProp prop = {};
    prop.type = CU_MEM_ALLOCATION_TYPE_PINNED;
    prop.location.type = CU_MEM_LOCATION_TYPE_DEVICE;
    prop.location.id = device;
    prop.requestedHandleTypes = CU_MEM_HANDLE_TYPE_POSIX_FILE_DESCRIPTOR;

    // 5. Allocate and align size
    const size_t buffer_size = 1024 * 1024; // 1MB
    const size_t aligned_size = alignSize(buffer_size, granularity);
    printf("Buffer size: %zu bytes, aligned size: %zu bytes\n", buffer_size, aligned_size);

    // 6. Create physical memory allocation
    CUmemGenericAllocationHandle alloc_handle;
    CHECK_CUDA(cuMemCreate(&alloc_handle, aligned_size, &prop, 0));
    printf("Created physical memory allocation\n");

    // 7. Reserve virtual address space
    CUdeviceptr dptr;
    CHECK_CUDA(cuMemAddressReserve(&dptr, aligned_size, 0, 0, 0));
    printf("Reserved virtual address space at 0x%llx\n", (unsigned long long)dptr);

    // 8. Map physical memory to virtual address
    CHECK_CUDA(cuMemMap(dptr, aligned_size, 0, alloc_handle, 0));
    printf("Mapped physical memory to virtual address\n");

    // 9. Set access permissions
    CUmemAccessDesc accessDesc = {};
    accessDesc.location.type = CU_MEM_LOCATION_TYPE_DEVICE;
    accessDesc.location.id = device;
    accessDesc.flags = CU_MEM_ACCESS_FLAGS_PROT_READWRITE;
    CHECK_CUDA(cuMemSetAccess(dptr, aligned_size, &accessDesc, 1));
    printf("Set read/write access permissions\n");

    // 10. Generate test data
    const size_t element_count = buffer_size / sizeof(int);
    std::vector<int> h_buffer(element_count);
    generateTestData(h_buffer.data(), element_count);
    printf("Generated %zu test integers\n", element_count);

    // 11. Copy data to GPU
    copyHostToDevice(dptr, h_buffer.data(), buffer_size);
    printf("Copied test data to GPU\n");

    // 12. Export as file descriptor
    int fd;
    CHECK_CUDA(cuMemExportToShareableHandle((void*)&fd, alloc_handle,
        CU_MEM_HANDLE_TYPE_POSIX_FILE_DESCRIPTOR, 0));
    printf("Exported allocation as FD: %d\n", fd);

    // 13. Setup IPC socket
    IPCSocket ipc_sock;
    if (ipc_sock.create_and_listen() < 0) {
        fprintf(stderr, "Failed to create IPC socket\n");
        return 1;
    }
    printf("Waiting for consumer connection...\n");

    // 14. Accept consumer connection
    if (ipc_sock.accept_connection() < 0) {
        fprintf(stderr, "Failed to accept consumer\n");
        return 1;
    }
    printf("Consumer connected\n");

    // 15. Send FD and metadata
    if (ipc_sock.send_fd(fd) < 0) {
        fprintf(stderr, "Failed to send FD\n");
        return 1;
    }
    if (ipc_sock.send_metadata(aligned_size) < 0) {
        fprintf(stderr, "Failed to send metadata\n");
        return 1;
    }
    printf("Sent FD and size metadata to consumer\n");

    // 16. Wait for consumer ACK
    if (ipc_sock.wait_ack() < 0) {
        fprintf(stderr, "Failed to receive ACK\n");
        return 1;
    }
    printf("Consumer verified data successfully!\n");

    // 17. Cleanup
    ::close(fd);
    CHECK_CUDA(cuMemUnmap(dptr, aligned_size));
    CHECK_CUDA(cuMemAddressFree(dptr, aligned_size));
    CHECK_CUDA(cuMemRelease(alloc_handle));
    CHECK_CUDA(cuDevicePrimaryCtxRelease(device));
    printf("Cleanup complete\n");

    return 0;
}
