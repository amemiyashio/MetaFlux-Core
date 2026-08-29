#define _GNU_SOURCE

#include "metaflux/cuda/passthrough.h"

#include "passthrough_internal.h"

#include <dirent.h>
#include <dlfcn.h>
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct test_environment {
  char root[PATH_MAX];
  char config[PATH_MAX];
  char missing_config[PATH_MAX];
  char proc_version[PATH_MAX];
  char pid_namespace[PATH_MAX];
  char mount_namespace[PATH_MAX];
  char install_root[PATH_MAX];
  const char* cuda_good;
  const char* nvml_good;
  const char* shadow_provider;
  const char* cuda_wrong_soname;
  const char* cuda_missing_bootstrap;
  const char* nvml_mismatched;
} test_environment;

typedef struct managed_state {
  mf_cuda_passthrough_status_v1 prepare_status;
  mf_cuda_passthrough_status_v1 commit_status;
  mf_cuda_passthrough_status_v1 rollback_status;
  uint32_t leave_dirty;
  uint32_t dirty;
  uint32_t prepare_calls;
  uint32_t commit_calls;
  uint32_t rollback_calls;
  uint32_t pristine_calls;
} managed_state;

static int remove_entry(const char* path, const struct stat* attributes, int type,
                        struct FTW* walk) {
  (void)attributes;
  (void)type;
  (void)walk;
  return remove(path);
}

static void remove_tree(const char* path) {
  if (path != NULL && path[0] != '\0') {
    (void)nftw(path, remove_entry, 32, FTW_DEPTH | FTW_PHYS);
  }
}

static int count_open_descriptors(void) {
  DIR* directory = opendir("/proc/self/fd");
  struct dirent* entry = NULL;
  int count = 0;
  if (directory == NULL) {
    return -1;
  }
  while ((entry = readdir(directory)) != NULL) {
    if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
      ++count;
    }
  }
  return closedir(directory) == 0 ? count : -1;
}

static int join_path(char output[PATH_MAX], const char* directory, const char* name) {
  const int written = snprintf(output, PATH_MAX, "%s/%s", directory, name);
  return written > 0 && written < PATH_MAX;
}

static int write_bytes(const char* path, const void* bytes, size_t byte_count) {
  const uint8_t* input = (const uint8_t*)bytes;
  size_t completed = 0;
  int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, S_IRUSR | S_IWUSR);
  if (fd < 0) {
    return 0;
  }
  while (completed < byte_count) {
    const ssize_t result = write(fd, input + completed, byte_count - completed);
    if (result < 0 && errno == EINTR) {
      continue;
    }
    if (result <= 0) {
      (void)close(fd);
      return 0;
    }
    completed += (size_t)result;
  }
  if (close(fd) != 0) {
    return 0;
  }
  return 1;
}

static int write_text(const char* path, const char* text) {
  return write_bytes(path, text, strlen(text));
}

static int patch_bytes(const char* path, const void* bytes, size_t byte_count, off_t offset) {
  const uint8_t* input = (const uint8_t*)bytes;
  size_t completed = 0;
  int fd = open(path, O_WRONLY | O_CLOEXEC);
  if (fd < 0) {
    return 0;
  }
  while (completed < byte_count) {
    const ssize_t result =
        pwrite(fd, input + completed, byte_count - completed, offset + (off_t)completed);
    if (result < 0 && errno == EINTR) {
      continue;
    }
    if (result <= 0) {
      (void)close(fd);
      return 0;
    }
    completed += (size_t)result;
  }
  return close(fd) == 0;
}

static int copy_file(const char* source, const char* destination) {
  uint8_t buffer[16384];
  int input = open(source, O_RDONLY | O_CLOEXEC);
  int output = -1;
  if (input < 0) {
    return 0;
  }
  output = open(destination, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, S_IRUSR | S_IWUSR);
  if (output < 0) {
    (void)close(input);
    return 0;
  }
  for (;;) {
    ssize_t read_size = read(input, buffer, sizeof(buffer));
    size_t written = 0;
    if (read_size < 0 && errno == EINTR) {
      continue;
    }
    if (read_size < 0) {
      (void)close(output);
      (void)close(input);
      return 0;
    }
    if (read_size == 0) {
      break;
    }
    while (written < (size_t)read_size) {
      const ssize_t result = write(output, buffer + written, (size_t)read_size - written);
      if (result < 0 && errno == EINTR) {
        continue;
      }
      if (result <= 0) {
        (void)close(output);
        (void)close(input);
        return 0;
      }
      written += (size_t)result;
    }
  }
  {
    const int output_status = close(output);
    const int input_status = close(input);
    return output_status == 0 && input_status == 0;
  }
}

static int write_config(const test_environment* environment, const char* cuda_path,
                        const char* nvml_path) {
  char contents[4096];
  const int written =
      snprintf(contents, sizeof(contents), "cuda=%s\nnvml=%s\n", cuda_path, nvml_path);
  return written > 0 && (size_t)written < sizeof(contents) &&
         write_bytes(environment->config, contents, (size_t)written);
}

static int initialize_environment(test_environment* environment, char** argv) {
  char template_path[] = "/tmp/metaflux-passthrough-XXXXXX";
  char* root = mkdtemp(template_path);
  if (root == NULL || strlen(root) >= sizeof(environment->root)) {
    return 0;
  }
  memset(environment, 0, sizeof(*environment));
  (void)memcpy(environment->root, root, strlen(root) + (size_t)1);
  if (!join_path(environment->config, root, "vendor-libraries.conf") ||
      !join_path(environment->missing_config, root, "no-vendor-libraries.conf") ||
      !join_path(environment->proc_version, root, "nvidia-version") ||
      !join_path(environment->pid_namespace, root, "pid-namespace") ||
      !join_path(environment->mount_namespace, root, "mount-namespace") ||
      !join_path(environment->install_root, root, "install-root") ||
      mkdir(environment->install_root, S_IRWXU) != 0 ||
      !write_text(environment->proc_version,
                  "NVRM version: NVIDIA UNIX x86_64 Kernel Module  777.42.01  Test Build\n") ||
      !write_text(environment->pid_namespace, "pid-a\n") ||
      !write_text(environment->mount_namespace, "mount-a\n")) {
    remove_tree(root);
    return 0;
  }
  environment->cuda_good = argv[1];
  environment->nvml_good = argv[2];
  environment->shadow_provider = argv[3];
  environment->cuda_wrong_soname = argv[4];
  environment->cuda_missing_bootstrap = argv[5];
  environment->nvml_mismatched = argv[6];
  return 1;
}

static void initialize_policy(const test_environment* environment,
                              mf_cuda_passthrough_policy_v1* policy, int use_config) {
  mf_cuda_passthrough_test_policy_init_v1(policy);
  policy->trusted_uid = (uint32_t)geteuid();
  policy->enforce_trusted_ancestors = UINT32_C(0);
  policy->config_path = use_config != 0 ? environment->config : environment->missing_config;
  policy->proc_version_path = environment->proc_version;
  policy->pid_namespace_path = environment->pid_namespace;
  policy->mount_namespace_path = environment->mount_namespace;
  policy->install_root = environment->install_root;
  policy->provider_self_path = environment->shadow_provider;
  policy->default_pair_count = UINT32_C(1);
  policy->default_cuda_paths[0] = environment->cuda_good;
  policy->default_nvml_paths[0] = environment->nvml_good;
  policy->default_cuda_paths[1] = NULL;
  policy->default_nvml_paths[1] = NULL;
}

static mf_cuda_passthrough_status_v1 managed_prepare(void* context, uint64_t* out_ticket) {
  managed_state* state = (managed_state*)context;
  ++state->prepare_calls;
  state->dirty = UINT32_C(1);
  *out_ticket = UINT64_C(17);
  return state->prepare_status;
}

static mf_cuda_passthrough_status_v1 managed_commit(void* context, uint64_t ticket) {
  managed_state* state = (managed_state*)context;
  ++state->commit_calls;
  return ticket == UINT64_C(17) ? state->commit_status : MF_CUDA_PASSTHROUGH_INVALID_ARGUMENT;
}

static mf_cuda_passthrough_status_v1 managed_rollback(void* context, uint64_t ticket) {
  managed_state* state = (managed_state*)context;
  ++state->rollback_calls;
  if (ticket == UINT64_C(17) && state->leave_dirty == UINT32_C(0)) {
    state->dirty = UINT32_C(0);
  }
  return state->rollback_status;
}

static int32_t managed_is_pristine(void* context) {
  managed_state* state = (managed_state*)context;
  ++state->pristine_calls;
  return state->dirty == UINT32_C(0) ? INT32_C(1) : INT32_C(0);
}

static mf_cuda_managed_callbacks_v1 callbacks_for(managed_state* state) {
  mf_cuda_managed_callbacks_v1 callbacks;
  memset(&callbacks, 0, sizeof(callbacks));
  callbacks.struct_size = sizeof(callbacks);
  callbacks.abi_version = MF_CUDA_PASSTHROUGH_ABI_VERSION_V1;
  callbacks.context = state;
  callbacks.prepare = managed_prepare;
  callbacks.commit = managed_commit;
  callbacks.rollback = managed_rollback;
  callbacks.is_pristine = managed_is_pristine;
  return callbacks;
}

static int test_mode_parser(void) {
  mf_cuda_runtime_mode_v1 mode = UINT32_C(0);
  mf_cuda_passthrough_policy_v1 defaults;
  mf_cuda_passthrough_test_policy_init_v1(&defaults);
  if (defaults.trusted_uid != UINT32_C(0) || defaults.enforce_trusted_ancestors != UINT32_C(1) ||
      strcmp(defaults.config_path, "/etc/metaflux/vendor-libraries.conf") != 0 ||
      strcmp(defaults.proc_version_path, "/proc/driver/nvidia/version") != 0 ||
      strcmp(defaults.pid_namespace_path, "/proc/self/ns/pid") != 0 ||
      strcmp(defaults.mount_namespace_path, "/proc/self/ns/mnt") != 0 ||
      strcmp(defaults.default_cuda_paths[0], "/usr/lib/x86_64-linux-gnu/libcuda.so.1") != 0 ||
      strcmp(defaults.default_nvml_paths[0], "/usr/lib/x86_64-linux-gnu/libnvidia-ml.so.1") != 0 ||
      strcmp(defaults.default_cuda_paths[1], "/usr/lib64/libcuda.so.1") != 0 ||
      strcmp(defaults.default_nvml_paths[1], "/usr/lib64/libnvidia-ml.so.1") != 0) {
    return 1;
  }
  if (mf_cuda_runtime_mode_parse_v1("managed", &mode) != MF_CUDA_PASSTHROUGH_SUCCESS ||
      mode != MF_CUDA_RUNTIME_MODE_MANAGED_V1 ||
      mf_cuda_runtime_mode_parse_v1("passthrough", &mode) != MF_CUDA_PASSTHROUGH_SUCCESS ||
      mode != MF_CUDA_RUNTIME_MODE_PASSTHROUGH_V1 ||
      mf_cuda_runtime_mode_parse_v1("auto", &mode) != MF_CUDA_PASSTHROUGH_SUCCESS ||
      mode != MF_CUDA_RUNTIME_MODE_AUTO_V1) {
    return 2;
  }
  if (mf_cuda_runtime_mode_parse_v1("Auto", &mode) != MF_CUDA_PASSTHROUGH_INVALID_MODE ||
      mf_cuda_runtime_mode_parse_v1(" auto", &mode) != MF_CUDA_PASSTHROUGH_INVALID_MODE ||
      mf_cuda_runtime_mode_parse_v1("auto\n", &mode) != MF_CUDA_PASSTHROUGH_INVALID_MODE ||
      mf_cuda_runtime_mode_parse_v1("", &mode) != MF_CUDA_PASSTHROUGH_INVALID_MODE ||
      mf_cuda_runtime_mode_parse_v1(NULL, &mode) != MF_CUDA_PASSTHROUGH_INVALID_ARGUMENT ||
      mf_cuda_runtime_mode_parse_v1("auto", NULL) != MF_CUDA_PASSTHROUGH_INVALID_ARGUMENT) {
    return 3;
  }
  return 0;
}

static int verify_loaded_pair(mf_cuda_passthrough_pair_v1* pair) {
  const mf_cuda_passthrough_bootstrap_v1* bootstrap = mf_cuda_passthrough_pair_bootstrap_v1(pair);
  void* symbol = NULL;
  typedef int (*driver_version_function)(int*);
  driver_version_function function = NULL;
  int version = 0;
  _Static_assert(sizeof(function) == sizeof(symbol), "POSIX function and data pointers must match");
  (void)dlerror();
  if (pair == NULL || bootstrap == NULL || bootstrap->struct_size != sizeof(*bootstrap) ||
      bootstrap->abi_version != MF_CUDA_PASSTHROUGH_ABI_VERSION_V1 || bootstrap->cu_init == NULL ||
      bootstrap->cu_driver_get_version == NULL || bootstrap->cu_get_proc_address == NULL ||
      bootstrap->nvml_init_v2 == NULL || bootstrap->nvml_shutdown == NULL ||
      bootstrap->nvml_system_get_driver_version == NULL ||
      strcmp(mf_cuda_passthrough_pair_driver_build_v1(pair), "777.42.01") != 0 ||
      mf_cuda_passthrough_pair_namespace_id_v1(pair) <= INT64_C(0) ||
      dlsym(RTLD_DEFAULT, "cuInit") == bootstrap->cu_init ||
      dlsym(RTLD_DEFAULT, "nvmlInit_v2") == bootstrap->nvml_init_v2 ||
      mf_cuda_passthrough_pair_validate_current_v1(pair) != MF_CUDA_PASSTHROUGH_SUCCESS ||
      mf_cuda_passthrough_pair_lookup_v1(pair, MF_CUDA_VENDOR_LIBRARY_CUDA_V1, "cuDriverGetVersion",
                                         "libcuda.so.1", &symbol) != MF_CUDA_PASSTHROUGH_SUCCESS ||
      symbol == NULL) {
    return 0;
  }
  memcpy(&function, &symbol, sizeof(function));
  if (function(&version) != 0 || version != 12070 ||
      mf_cuda_passthrough_pair_lookup_v1(pair, MF_CUDA_VENDOR_LIBRARY_CUDA_V1, "missing",
                                         "libcuda.so.1",
                                         &symbol) != MF_CUDA_PASSTHROUGH_SYMBOL_MISSING) {
    return 0;
  }
  return 1;
}

static int test_valid_discovery(const test_environment* environment) {
  mf_cuda_passthrough_policy_v1 policy;
  mf_cuda_passthrough_pair_v1* pair = NULL;
  mf_cuda_passthrough_status_v1 status = MF_CUDA_PASSTHROUGH_SUCCESS;
  initialize_policy(environment, &policy, 1);
  if (!write_config(environment, environment->cuda_good, environment->nvml_good)) {
    return 1;
  }
  status = mf_cuda_passthrough_pair_load_with_policy_v1(&policy, &pair);
  if (status != MF_CUDA_PASSTHROUGH_SUCCESS || !verify_loaded_pair(pair)) {
    (void)fprintf(stderr, "configured discovery failed: status=%u pair=%p\n", status, (void*)pair);
    mf_cuda_passthrough_pair_release_v1(pair);
    return 1;
  }
  mf_cuda_passthrough_pair_release_v1(pair);
  pair = NULL;
  initialize_policy(environment, &policy, 0);
  status = mf_cuda_passthrough_pair_load_with_policy_v1(&policy, &pair);
  if (status != MF_CUDA_PASSTHROUGH_SUCCESS || !verify_loaded_pair(pair)) {
    (void)fprintf(stderr, "default discovery failed: status=%u pair=%p\n", status, (void*)pair);
    mf_cuda_passthrough_pair_release_v1(pair);
    return 2;
  }
  mf_cuda_passthrough_pair_release_v1(pair);
  pair = NULL;
  policy.default_pair_count = UINT32_C(2);
  policy.default_cuda_paths[0] = environment->missing_config;
  policy.default_nvml_paths[0] = environment->missing_config;
  policy.default_cuda_paths[1] = environment->cuda_good;
  policy.default_nvml_paths[1] = environment->nvml_good;
  status = mf_cuda_passthrough_pair_load_with_policy_v1(&policy, &pair);
  if (status != MF_CUDA_PASSTHROUGH_SUCCESS || !verify_loaded_pair(pair)) {
    mf_cuda_passthrough_pair_release_v1(pair);
    return 3;
  }
  mf_cuda_passthrough_pair_release_v1(pair);
  return 0;
}

static int expect_load_status(const test_environment* environment,
                              mf_cuda_passthrough_policy_v1* policy, const char* cuda_path,
                              const char* nvml_path, mf_cuda_passthrough_status_v1 expected) {
  mf_cuda_passthrough_pair_v1* pair = NULL;
  if (!write_config(environment, cuda_path, nvml_path)) {
    return 0;
  }
  if (mf_cuda_passthrough_pair_load_with_policy_v1(policy, &pair) != expected || pair != NULL) {
    mf_cuda_passthrough_pair_release_v1(pair);
    return 0;
  }
  return 1;
}

static int expect_config_status(const test_environment* environment,
                                mf_cuda_passthrough_policy_v1* policy, const char* contents,
                                mf_cuda_passthrough_status_v1 expected) {
  mf_cuda_passthrough_pair_v1* pair = NULL;
  if (!write_text(environment->config, contents) ||
      mf_cuda_passthrough_pair_load_with_policy_v1(policy, &pair) != expected || pair != NULL) {
    mf_cuda_passthrough_pair_release_v1(pair);
    return 0;
  }
  return 1;
}

static int test_policy_faults(test_environment* environment) {
  mf_cuda_passthrough_policy_v1 policy;
  char config_contents[PATH_MAX + 64];
  char copied_self[PATH_MAX];
  char install_link[PATH_MAX];
  char installed_cuda[PATH_MAX];
  char writable_directory[PATH_MAX];
  char writable_cuda[PATH_MAX];
  char malformed_directory[PATH_MAX];
  char malformed_cuda[PATH_MAX];
  char truncated_directory[PATH_MAX];
  char truncated_cuda[PATH_MAX];
  char wrong_arch_directory[PATH_MAX];
  char wrong_arch_cuda[PATH_MAX];
  char invalid_offset_directory[PATH_MAX];
  char invalid_offset_cuda[PATH_MAX];
  uint16_t machine = EM_AARCH64;
  Elf64_Off program_offset = UINT64_MAX;
  uint32_t failure_index = 0;
  int descriptors_before = -1;
  int descriptors_after = -1;
  int written = 0;

  initialize_policy(environment, &policy, 1);
  if (!expect_load_status(environment, &policy, environment->cuda_wrong_soname,
                          environment->nvml_good, MF_CUDA_PASSTHROUGH_MALFORMED_ELF) ||
      !expect_load_status(environment, &policy, environment->cuda_good,
                          environment->nvml_mismatched, MF_CUDA_PASSTHROUGH_BUILD_MISMATCH)) {
    return 1;
  }
  if (!write_text(environment->proc_version,
                  "NVRM version: NVIDIA UNIX Kernel Module  999.1.2  Test\n") ||
      !expect_load_status(environment, &policy, environment->cuda_good, environment->nvml_good,
                          MF_CUDA_PASSTHROUGH_BUILD_MISMATCH) ||
      !write_text(environment->proc_version,
                  "NVRM version: NVIDIA UNIX x86_64 Kernel Module  777.42.01  Test Build\n")) {
    return 2;
  }
  {
    mf_cuda_passthrough_pair_v1* pair = NULL;
    mf_cuda_passthrough_status_v1 status = MF_CUDA_PASSTHROUGH_SUCCESS;
    if (!write_config(environment, environment->cuda_good, environment->nvml_good) ||
        chmod(environment->config, 0660) != 0) {
      return 3;
    }
    status = mf_cuda_passthrough_pair_load_with_policy_v1(&policy, &pair);
    if (chmod(environment->config, 0600) != 0 || status != MF_CUDA_PASSTHROUGH_POLICY_REJECTED ||
        pair != NULL) {
      mf_cuda_passthrough_pair_release_v1(pair);
      return 3;
    }
  }
  written = snprintf(config_contents, sizeof(config_contents), "cuda=libcuda.so.1\nnvml=%s\n",
                     environment->nvml_good);
  if (written <= 0 || (size_t)written >= sizeof(config_contents) ||
      !expect_config_status(environment, &policy, config_contents,
                            MF_CUDA_PASSTHROUGH_MALFORMED_CONFIG)) {
    return 3;
  }
  written = snprintf(config_contents, sizeof(config_contents), "cuda=%s\nnvml=libnvidia-ml.so.1\n",
                     environment->cuda_good);
  if (written <= 0 || (size_t)written >= sizeof(config_contents) ||
      !expect_config_status(environment, &policy, config_contents,
                            MF_CUDA_PASSTHROUGH_MALFORMED_CONFIG)) {
    return 3;
  }
  policy.enforce_trusted_ancestors = UINT32_C(1);
  if (!expect_load_status(environment, &policy, environment->cuda_good, environment->nvml_good,
                          MF_CUDA_PASSTHROUGH_POLICY_REJECTED)) {
    return 3;
  }
  policy.enforce_trusted_ancestors = UINT32_C(0);
  policy.trusted_uid = (uint32_t)geteuid() == UINT32_C(0) ? UINT32_C(1) : UINT32_C(0);
  if (!expect_load_status(environment, &policy, environment->cuda_good, environment->nvml_good,
                          MF_CUDA_PASSTHROUGH_POLICY_REJECTED)) {
    return 3;
  }
  policy.trusted_uid = (uint32_t)geteuid();
  policy.provider_self_path = environment->cuda_good;
  if (!expect_load_status(environment, &policy, environment->cuda_good, environment->nvml_good,
                          MF_CUDA_PASSTHROUGH_POLICY_REJECTED)) {
    return 3;
  }
  policy.provider_self_path = environment->nvml_good;
  if (!expect_load_status(environment, &policy, environment->cuda_good, environment->nvml_good,
                          MF_CUDA_PASSTHROUGH_POLICY_REJECTED)) {
    return 3;
  }
  if (!join_path(copied_self, environment->root, "shadow-copy.so") ||
      !copy_file(environment->cuda_good, copied_self)) {
    return 4;
  }
  policy.provider_self_path = copied_self;
  if (!expect_load_status(environment, &policy, environment->cuda_good, environment->nvml_good,
                          MF_CUDA_PASSTHROUGH_POLICY_REJECTED)) {
    return 5;
  }
  policy.provider_self_path = environment->shadow_provider;
  {
    char* canonical = realpath(environment->cuda_good, NULL);
    char* slash = canonical == NULL ? NULL : strrchr(canonical, '/');
    if (slash == NULL) {
      free(canonical);
      return 6;
    }
    *slash = '\0';
    policy.install_root = canonical;
    if (!expect_load_status(environment, &policy, environment->cuda_good, environment->nvml_good,
                            MF_CUDA_PASSTHROUGH_POLICY_REJECTED)) {
      free(canonical);
      return 7;
    }
    free(canonical);
    policy.install_root = environment->install_root;
  }
  if (!join_path(install_link, environment->root, "install-link") ||
      !join_path(installed_cuda, environment->install_root, "libcuda.so.777.42.01") ||
      !copy_file(environment->cuda_good, installed_cuda) ||
      symlink(environment->install_root, install_link) != 0) {
    return 7;
  }
  policy.install_root = install_link;
  if (!expect_load_status(environment, &policy, installed_cuda, environment->nvml_good,
                          MF_CUDA_PASSTHROUGH_POLICY_REJECTED)) {
    return 7;
  }
  policy.install_root = environment->install_root;

  if (!join_path(writable_directory, environment->root, "writable") ||
      mkdir(writable_directory, S_IRWXU) != 0 ||
      !join_path(writable_cuda, writable_directory, "libcuda.so.777.42.01") ||
      !copy_file(environment->cuda_good, writable_cuda) || chmod(writable_cuda, 0660) != 0 ||
      !expect_load_status(environment, &policy, writable_cuda, environment->nvml_good,
                          MF_CUDA_PASSTHROUGH_POLICY_REJECTED)) {
    return 8;
  }
  if (!join_path(malformed_directory, environment->root, "nonregular") ||
      mkdir(malformed_directory, S_IRWXU) != 0 ||
      !join_path(malformed_cuda, malformed_directory, "libcuda.so.777.42.01") ||
      mkdir(malformed_cuda, S_IRWXU) != 0 ||
      !expect_load_status(environment, &policy, malformed_cuda, environment->nvml_good,
                          MF_CUDA_PASSTHROUGH_POLICY_REJECTED)) {
    return 9;
  }
  if (!join_path(truncated_directory, environment->root, "truncated") ||
      mkdir(truncated_directory, S_IRWXU) != 0 ||
      !join_path(truncated_cuda, truncated_directory, "libcuda.so.777.42.01") ||
      !write_text(truncated_cuda, "not-an-elf") ||
      !expect_load_status(environment, &policy, truncated_cuda, environment->nvml_good,
                          MF_CUDA_PASSTHROUGH_MALFORMED_ELF)) {
    return 10;
  }
  if (!join_path(wrong_arch_directory, environment->root, "wrong-arch") ||
      mkdir(wrong_arch_directory, S_IRWXU) != 0 ||
      !join_path(wrong_arch_cuda, wrong_arch_directory, "libcuda.so.777.42.01") ||
      !copy_file(environment->cuda_good, wrong_arch_cuda)) {
    return 11;
  }
  if (!patch_bytes(wrong_arch_cuda, &machine, sizeof(machine),
                   (off_t)offsetof(Elf64_Ehdr, e_machine)) ||
      !expect_load_status(environment, &policy, wrong_arch_cuda, environment->nvml_good,
                          MF_CUDA_PASSTHROUGH_MALFORMED_ELF)) {
    return 12;
  }
  descriptors_before = count_open_descriptors();
  for (failure_index = 0; failure_index < UINT32_C(32); ++failure_index) {
    if (!expect_load_status(environment, &policy, environment->cuda_missing_bootstrap,
                            environment->nvml_good, MF_CUDA_PASSTHROUGH_SYMBOL_MISSING)) {
      return 13;
    }
  }
  descriptors_after = count_open_descriptors();
  if (descriptors_before < 0 || descriptors_after != descriptors_before) {
    return 13;
  }
  if (!join_path(invalid_offset_directory, environment->root, "invalid-offset") ||
      mkdir(invalid_offset_directory, S_IRWXU) != 0 ||
      !join_path(invalid_offset_cuda, invalid_offset_directory, "libcuda.so.777.42.01") ||
      !copy_file(environment->cuda_good, invalid_offset_cuda)) {
    return 14;
  }
  if (!patch_bytes(invalid_offset_cuda, &program_offset, sizeof(program_offset),
                   (off_t)offsetof(Elf64_Ehdr, e_phoff)) ||
      !expect_load_status(environment, &policy, invalid_offset_cuda, environment->nvml_good,
                          MF_CUDA_PASSTHROUGH_MALFORMED_ELF)) {
    return 14;
  }
  if (!write_text(environment->config, "cuda=/tmp/a\ncuda=/tmp/b\nnvml=/tmp/c\n")) {
    return 15;
  }
  {
    mf_cuda_passthrough_pair_v1* pair = NULL;
    if (mf_cuda_passthrough_pair_load_with_policy_v1(&policy, &pair) !=
            MF_CUDA_PASSTHROUGH_MALFORMED_CONFIG ||
        pair != NULL) {
      mf_cuda_passthrough_pair_release_v1(pair);
      return 16;
    }
  }
  return 0;
}

static int test_fingerprint_invalidation(test_environment* environment) {
  mf_cuda_passthrough_policy_v1 policy;
  mf_cuda_passthrough_pair_v1* pair = NULL;
  mf_cuda_passthrough_pair_v1* symlink_pair = NULL;
  char cuda_link[PATH_MAX];
  pid_t child = -1;
  int child_status = 0;
  void* stale_symbol = NULL;

  initialize_policy(environment, &policy, 1);
  if (!write_config(environment, environment->cuda_good, environment->nvml_good) ||
      mf_cuda_passthrough_pair_load_with_policy_v1(&policy, &pair) != MF_CUDA_PASSTHROUGH_SUCCESS) {
    return 1;
  }
  child = fork();
  if (child == 0) {
    _exit(mf_cuda_passthrough_pair_validate_current_v1(pair) == MF_CUDA_PASSTHROUGH_STALE ? 0 : 1);
  }
  if (child < 0 || waitpid(child, &child_status, 0) != child || !WIFEXITED(child_status) ||
      WEXITSTATUS(child_status) != 0) {
    mf_cuda_passthrough_pair_release_v1(pair);
    return 2;
  }
  stale_symbol = &policy;
  if (!write_text(environment->proc_version,
                  "NVRM version: NVIDIA UNIX Kernel Module  777.42.02  Changed Build extra\n") ||
      mf_cuda_passthrough_pair_validate_current_v1(pair) != MF_CUDA_PASSTHROUGH_STALE ||
      mf_cuda_passthrough_pair_lookup_v1(pair, MF_CUDA_VENDOR_LIBRARY_CUDA_V1, "cuInit",
                                         "libcuda.so.1",
                                         &stale_symbol) != MF_CUDA_PASSTHROUGH_STALE ||
      stale_symbol != NULL) {
    mf_cuda_passthrough_pair_release_v1(pair);
    return 3;
  }
  mf_cuda_passthrough_pair_release_v1(pair);
  pair = NULL;
  if (!write_text(environment->proc_version,
                  "NVRM version: NVIDIA UNIX x86_64 Kernel Module  777.42.01  Test Build\n") ||
      mf_cuda_passthrough_pair_load_with_policy_v1(&policy, &pair) != MF_CUDA_PASSTHROUGH_SUCCESS ||
      !write_text(environment->pid_namespace, "pid-namespace-changed\n") ||
      mf_cuda_passthrough_pair_validate_current_v1(pair) != MF_CUDA_PASSTHROUGH_STALE) {
    mf_cuda_passthrough_pair_release_v1(pair);
    return 4;
  }
  mf_cuda_passthrough_pair_release_v1(pair);
  pair = NULL;
  if (!write_text(environment->pid_namespace, "pid-a\n") ||
      mf_cuda_passthrough_pair_load_with_policy_v1(&policy, &pair) != MF_CUDA_PASSTHROUGH_SUCCESS ||
      !write_text(environment->mount_namespace, "mount-namespace-changed\n") ||
      mf_cuda_passthrough_pair_validate_current_v1(pair) != MF_CUDA_PASSTHROUGH_STALE) {
    mf_cuda_passthrough_pair_release_v1(pair);
    return 5;
  }
  mf_cuda_passthrough_pair_release_v1(pair);
  pair = NULL;
  if (!write_text(environment->mount_namespace, "mount-a\n") ||
      mf_cuda_passthrough_pair_load_with_policy_v1(&policy, &pair) != MF_CUDA_PASSTHROUGH_SUCCESS ||
      !write_text(environment->config, "cuda=/tmp/replaced\nnvml=/tmp/replaced\nextra=1\n") ||
      mf_cuda_passthrough_pair_validate_current_v1(pair) != MF_CUDA_PASSTHROUGH_STALE) {
    mf_cuda_passthrough_pair_release_v1(pair);
    return 6;
  }
  mf_cuda_passthrough_pair_release_v1(pair);
  pair = NULL;
  if (!join_path(cuda_link, environment->root, "libcuda.so.1") ||
      symlink(environment->cuda_good, cuda_link) != 0 ||
      !write_config(environment, cuda_link, environment->nvml_good) ||
      mf_cuda_passthrough_pair_load_with_policy_v1(&policy, &symlink_pair) !=
          MF_CUDA_PASSTHROUGH_SUCCESS ||
      unlink(cuda_link) != 0 || symlink(environment->cuda_wrong_soname, cuda_link) != 0 ||
      mf_cuda_passthrough_pair_validate_current_v1(symlink_pair) != MF_CUDA_PASSTHROUGH_STALE) {
    mf_cuda_passthrough_pair_release_v1(symlink_pair);
    return 7;
  }
  mf_cuda_passthrough_pair_release_v1(symlink_pair);
  return 0;
}

static int test_mode_state_machine(test_environment* environment) {
  mf_cuda_passthrough_policy_v1 policy;
  mf_cuda_passthrough_pair_v1* pair = NULL;
  mf_cuda_selected_runtime_v1 selected = MF_CUDA_SELECTED_NONE_V1;
  managed_state state;
  mf_cuda_managed_callbacks_v1 callbacks;

  initialize_policy(environment, &policy, 1);
  if (!write_config(environment, environment->cuda_good, environment->nvml_good)) {
    return 1;
  }
  memset(&state, 0, sizeof(state));
  state.prepare_status = MF_CUDA_PASSTHROUGH_SUCCESS;
  state.commit_status = MF_CUDA_PASSTHROUGH_SUCCESS;
  state.rollback_status = MF_CUDA_PASSTHROUGH_SUCCESS;
  callbacks = callbacks_for(&state);
  if (mf_cuda_runtime_select_with_policy_v1(MF_CUDA_RUNTIME_MODE_MANAGED_V1, &callbacks, &policy,
                                            &selected, &pair) != MF_CUDA_PASSTHROUGH_SUCCESS ||
      selected != MF_CUDA_SELECTED_MANAGED_V1 || pair != NULL ||
      state.prepare_calls != UINT32_C(1) || state.commit_calls != UINT32_C(1) ||
      state.rollback_calls != UINT32_C(0)) {
    return 2;
  }

  memset(&state, 0, sizeof(state));
  state.prepare_status = MF_CUDA_PASSTHROUGH_LOAD_FAILED;
  state.commit_status = MF_CUDA_PASSTHROUGH_SUCCESS;
  state.rollback_status = MF_CUDA_PASSTHROUGH_SUCCESS;
  callbacks = callbacks_for(&state);
  selected = MF_CUDA_SELECTED_NONE_V1;
  if (mf_cuda_runtime_select_with_policy_v1(MF_CUDA_RUNTIME_MODE_AUTO_V1, &callbacks, &policy,
                                            &selected, &pair) != MF_CUDA_PASSTHROUGH_SUCCESS ||
      selected != MF_CUDA_SELECTED_PASSTHROUGH_V1 || !verify_loaded_pair(pair) ||
      state.prepare_calls != UINT32_C(1) || state.commit_calls != UINT32_C(0) ||
      state.rollback_calls != UINT32_C(1) || state.pristine_calls != UINT32_C(1)) {
    mf_cuda_passthrough_pair_release_v1(pair);
    return 3;
  }
  mf_cuda_passthrough_pair_release_v1(pair);
  pair = NULL;

  memset(&state, 0, sizeof(state));
  state.prepare_status = MF_CUDA_PASSTHROUGH_SUCCESS;
  state.commit_status = MF_CUDA_PASSTHROUGH_LOAD_FAILED;
  state.rollback_status = MF_CUDA_PASSTHROUGH_SUCCESS;
  state.leave_dirty = UINT32_C(1);
  callbacks = callbacks_for(&state);
  selected = MF_CUDA_SELECTED_NONE_V1;
  if (mf_cuda_runtime_select_with_policy_v1(MF_CUDA_RUNTIME_MODE_AUTO_V1, &callbacks, &policy,
                                            &selected,
                                            &pair) != MF_CUDA_PASSTHROUGH_PARTIAL_STATE ||
      selected != MF_CUDA_SELECTED_NONE_V1 || pair != NULL || state.rollback_calls != UINT32_C(1) ||
      state.pristine_calls != UINT32_C(1)) {
    return 4;
  }

  memset(&state, 0, sizeof(state));
  state.prepare_status = MF_CUDA_PASSTHROUGH_LOAD_FAILED;
  state.rollback_status = MF_CUDA_PASSTHROUGH_SUCCESS;
  callbacks = callbacks_for(&state);
  if (mf_cuda_runtime_select_with_policy_v1(MF_CUDA_RUNTIME_MODE_MANAGED_V1, &callbacks, &policy,
                                            &selected, &pair) != MF_CUDA_PASSTHROUGH_LOAD_FAILED ||
      selected != MF_CUDA_SELECTED_NONE_V1 || pair != NULL || state.dirty != UINT32_C(0)) {
    return 5;
  }
  if (mf_cuda_runtime_select_with_policy_v1(MF_CUDA_RUNTIME_MODE_PASSTHROUGH_V1, NULL, &policy,
                                            &selected, &pair) != MF_CUDA_PASSTHROUGH_SUCCESS ||
      selected != MF_CUDA_SELECTED_PASSTHROUGH_V1 || !verify_loaded_pair(pair)) {
    mf_cuda_passthrough_pair_release_v1(pair);
    return 6;
  }
  mf_cuda_passthrough_pair_release_v1(pair);
  return 0;
}

int main(int argc, char** argv) {
  test_environment environment;
  int result = 0;
  if (argc != 7 || !initialize_environment(&environment, argv)) {
    return 64;
  }
  result = test_mode_parser();
  if (result == 0) {
    result = test_valid_discovery(&environment);
    if (result != 0) {
      result += 100;
    }
  }
  if (result == 0) {
    result = test_policy_faults(&environment);
    if (result != 0) {
      result += 200;
    }
  }
  if (result == 0) {
    result = test_fingerprint_invalidation(&environment);
    if (result != 0) {
      result += 300;
    }
  }
  if (result == 0) {
    result = test_mode_state_machine(&environment);
    if (result != 0) {
      result += 400;
    }
  }
  remove_tree(environment.root);
  return result;
}
