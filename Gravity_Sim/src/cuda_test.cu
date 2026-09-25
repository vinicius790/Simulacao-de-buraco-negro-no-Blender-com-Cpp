#include <stdio.h>
#include <cuda_runtime.h>

__global__ void testKernel() {
    printf("Hello from GPU!\n");
}

int main() {
    printf("CUDA test program\n");
    
    int deviceCount = 0;
    cudaGetDeviceCount(&deviceCount);
    
    if (deviceCount == 0) {
        printf("No CUDA devices found!\n");
        return 1;
    }
    
    printf("Found %d CUDA device(s)\n", deviceCount);
    
    for (int i = 0; i < deviceCount; i++) {
        cudaDeviceProp prop;
        cudaGetDeviceProperties(&prop, i);
        printf("Device %d: %s\n", i, prop.name);
    }
    
    testKernel<<<1, 1>>>();
    cudaDeviceSynchronize();
    
    return 0;
}