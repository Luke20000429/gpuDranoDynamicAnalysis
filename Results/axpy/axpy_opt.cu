#include <cuda_runtime.h> 
#include <iostream>


#define checkCudaErrors(call)                                 \
  do {                                                        \
    cudaError_t err = call;                                   \
    if (err != cudaSuccess) {                                 \
      printf("CUDA error at %s %d: %s\n", __FILE__, __LINE__, \
             cudaGetErrorString(err));                        \
      exit(EXIT_FAILURE);                                     \
    }                                                         \
  } while (0)


__global__ void axpy(float a, float* x, float* y) {
  y[threadIdx.x] = a * x[threadIdx.x];
}

int main(int argc, char* argv[]) {
  const int kDataLen = 4;

  float a = 2.0f;
  float host_x[kDataLen] = {1.0f, 2.0f, 3.0f, 4.0f};

  // Copy input data to device.
  float* x;
  float* y;
  checkCudaErrors(cudaMallocManaged(&x, kDataLen * sizeof(float)));
  checkCudaErrors(cudaMallocManaged(&y, kDataLen * sizeof(float)));
  for (int i = 0; i < kDataLen; ++i) {
    x[i] = host_x[i];
  }

  int device = -1;
  cudaGetDevice(&device);
  cudaMemPrefetchAsync(x, kDataLen * sizeof(float), device, NULL);
  cudaMemPrefetchAsync(y, kDataLen * sizeof(float), device, NULL);

  // Launch the kernel.
  axpy<<<1, kDataLen>>>(a, x, y);

  // Copy output data to host.
  checkCudaErrors(cudaDeviceSynchronize());

  // Print the results.
  for (int i = 0; i < kDataLen; ++i) {
    std::cout << "y[" << i << "] = " << y[i] << "\n";
  }

  checkCudaErrors(cudaDeviceReset());
  return 0;
}

