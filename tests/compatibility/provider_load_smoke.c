#include <dlfcn.h>
#include <stddef.h>

int main(int argc, char** argv) {
  void* cuda_handle;
  void* nvml_handle;

  if (argc != 3) {
    return 2;
  }

  cuda_handle = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
  if (cuda_handle == NULL) {
    return 1;
  }

  nvml_handle = dlopen(argv[2], RTLD_NOW | RTLD_LOCAL);
  if (nvml_handle == NULL) {
    (void)dlclose(cuda_handle);
    return 1;
  }

  if (dlclose(nvml_handle) != 0) {
    (void)dlclose(cuda_handle);
    return 1;
  }
  return dlclose(cuda_handle) == 0 ? 0 : 1;
}
