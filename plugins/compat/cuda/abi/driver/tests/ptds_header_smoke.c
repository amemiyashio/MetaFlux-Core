#define CUDA_API_PER_THREAD_DEFAULT_STREAM 1
#include <cuda.h>

int main(void) {
  CUresult (*const query)(CUstream) = cuStreamQuery;
  CUresult (*const synchronize)(CUstream) = cuStreamSynchronize;
  CUresult (*const record)(CUevent, CUstream) = cuEventRecord;
  return query == (CUresult (*)(CUstream))0 || synchronize == (CUresult (*)(CUstream))0 ||
                 record == (CUresult (*)(CUevent, CUstream))0
             ? 1
             : 0;
}
