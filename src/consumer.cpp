#include "cuda_ipc_common.h"
#include "ipc_socket.h"
#include <vector>
#include <unistd.h>

int main() {
    printf("=== CUDA VMM Consumer ===\n");

    // 1. Initialize CUDA
    CUdevice device = initCudaDevice(0);
    CUcontext context = createCudaContext(device);

    // 2. Connect to producer
    IPCSocket ipc_sock;
    printf("Connecting to producer...\n");
    if (ipc_sock.connect_to_server() < 0) {
        fprintf(stderr, "Failed to connect to producer\n");
        return 1;
    }
    printf("Connected to producer\n");

    // 3. Receive FD and metadata
    int received_fd;
    size_t aligned_size;
    if (ipc_sock.recv_fd(received_fd) < 0) {
        fprintf(stderr, "Failed to receive FD\n");
        return 1;
    }
    if (ipc_sock.recv_metadata(aligned_size) < 0) {
        fprintf(stderr, "Failed to receive metadata\n");
        return 1;
    }
    printf("Received FD: %d, size: %zu bytes\n", received_fd, aligned_size);

    // 4. Import handle from FD
    CUmemGenericAllocationHandle imported_handle;
    CHECK_CUDA(cuMemImportFromShareableHandle(&imported_handle,
        (void*)(intptr_t)received_fd,
        CU_MEM_HANDLE_TYPE_POSIX_FILE_DESCRIPTOR));
    printf("Imported allocation handle from FD\n");

    // 5. Reserve virtual address space
    CUdeviceptr consumer_dptr;
    CHECK_CUDA(cuMemAddressReserve(&consumer_dptr, aligned_size, 0, 0, 0));
    printf("Reserved virtual address space at 0x%llx\n", (unsigned long long)consumer_dptr);

    // 6. Map imported physical memory
    CHECK_CUDA(cuMemMap(consumer_dptr, aligned_size, 0, imported_handle, 0));
    printf("Mapped imported memory to virtual address\n");

    // 7. Set access permissions
    CUmemAccessDesc accessDesc = {};
    accessDesc.location.type = CU_MEM_LOCATION_TYPE_DEVICE;
    accessDesc.location.id = device;
    accessDesc.flags = CU_MEM_ACCESS_FLAGS_PROT_READWRITE;
    CHECK_CUDA(cuMemSetAccess(consumer_dptr, aligned_size, &accessDesc, 1));
    printf("Set read/write access permissions\n");

    // 8. Copy data from GPU to host
    const size_t buffer_size = 1024 * 1024; // Same as producer
    const size_t element_count = buffer_size / sizeof(int);
    std::vector<int> h_buffer(element_count);

    copyDeviceToHost(h_buffer.data(), consumer_dptr, buffer_size);
    printf("Copied %zu bytes from GPU to host\n", buffer_size);

    // 9. Verify data
    bool success = verifyTestData(h_buffer.data(), element_count);
    if (success) {
        printf("Data verification PASSED (%zu integers verified)\n", element_count);
    } else {
        printf("Data verification FAILED\n");
    }

    // 10. Send ACK
    if (ipc_sock.send_ack() < 0) {
        fprintf(stderr, "Failed to send ACK\n");
        return 1;
    }
    printf("Sent acknowledgment to producer\n");

    // 11. Cleanup
    CHECK_CUDA(cuMemUnmap(consumer_dptr, aligned_size));
    CHECK_CUDA(cuMemAddressFree(consumer_dptr, aligned_size));
    CHECK_CUDA(cuMemRelease(imported_handle));
    ::close(received_fd);
    CHECK_CUDA(cuDevicePrimaryCtxRelease(device));
    printf("Cleanup complete\n");

    return success ? 0 : 1;
}