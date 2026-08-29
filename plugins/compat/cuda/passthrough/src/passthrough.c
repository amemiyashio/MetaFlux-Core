#define _GNU_SOURCE

#include "metaflux/cuda/passthrough.h"

#include "passthrough_internal.h"

#include <dlfcn.h>
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <link.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef MF_CUDA_PASSTHROUGH_INSTALL_ROOT
#define MF_CUDA_PASSTHROUGH_INSTALL_ROOT "/opt/metaflux"
#endif

#define MF_PT_MAX_BUILD_ID UINT32_C(64)
#define MF_PT_MAX_BUILD_TEXT UINT32_C(63)
#define MF_PT_MAX_SONAME UINT32_C(127)
#define MF_PT_MAX_CONFIG_SIZE UINT32_C(4096)
#define MF_PT_MAX_PROC_VERSION_SIZE UINT32_C(4096)
#define MF_PT_MAX_PROGRAM_HEADERS UINT32_C(1024)
#define MF_PT_MAX_DYNAMIC_ENTRIES UINT32_C(1048576)

typedef struct mf_pt_stat_stamp {
  uint64_t device;
  uint64_t inode;
  uint64_t size;
  int64_t modified_seconds;
  int64_t modified_nanoseconds;
  uint32_t mode;
  uint32_t uid;
} mf_pt_stat_stamp;

typedef struct mf_pt_path_snapshot {
  uint32_t exists;
  mf_pt_stat_stamp source;
  mf_pt_stat_stamp target;
  char* resolved_path;
} mf_pt_path_snapshot;

typedef struct mf_pt_elf_identity {
  char soname[MF_PT_MAX_SONAME + UINT32_C(1)];
  uint8_t build_id[MF_PT_MAX_BUILD_ID];
  uint32_t build_id_size;
} mf_pt_elf_identity;

typedef struct mf_pt_library_record {
  char* source_path;
  mf_pt_path_snapshot path;
  mf_pt_elf_identity elf;
  char driver_build[MF_PT_MAX_BUILD_TEXT + UINT32_C(1)];
  int fd;
} mf_pt_library_record;

struct mf_cuda_passthrough_pair_v1 {
  mf_pt_library_record cuda;
  mf_pt_library_record nvml;
  mf_pt_path_snapshot config;
  mf_pt_path_snapshot proc_version;
  mf_pt_path_snapshot pid_namespace;
  mf_pt_path_snapshot mount_namespace;
  char* config_path;
  char* proc_version_path;
  char* pid_namespace_path;
  char* mount_namespace_path;
  char proc_driver_build[MF_PT_MAX_BUILD_TEXT + UINT32_C(1)];
  pid_t owner_pid;
  uint32_t trusted_uid;
  uint32_t enforce_trusted_ancestors;
  void* cuda_handle;
  void* nvml_handle;
  Lmid_t namespace_id;
  mf_cuda_passthrough_bootstrap_v1 bootstrap;
};

static const char* const mf_pt_default_cuda_paths[MF_CUDA_PASSTHROUGH_MAX_DEFAULT_PAIRS] = {
    "/usr/lib/x86_64-linux-gnu/libcuda.so.1",
    "/usr/lib64/libcuda.so.1",
};

static const char* const mf_pt_default_nvml_paths[MF_CUDA_PASSTHROUGH_MAX_DEFAULT_PAIRS] = {
    "/usr/lib/x86_64-linux-gnu/libnvidia-ml.so.1",
    "/usr/lib64/libnvidia-ml.so.1",
};

static void mf_pt_stat_from_native(const struct stat* source, mf_pt_stat_stamp* destination) {
  destination->device = (uint64_t)source->st_dev;
  destination->inode = (uint64_t)source->st_ino;
  destination->size = source->st_size < 0 ? UINT64_C(0) : (uint64_t)source->st_size;
  destination->modified_seconds = (int64_t)source->st_mtim.tv_sec;
  destination->modified_nanoseconds = (int64_t)source->st_mtim.tv_nsec;
  destination->mode = (uint32_t)source->st_mode;
  destination->uid = (uint32_t)source->st_uid;
}

static int mf_pt_stat_equal(const mf_pt_stat_stamp* left, const mf_pt_stat_stamp* right) {
  return left->device == right->device && left->inode == right->inode &&
         left->size == right->size && left->modified_seconds == right->modified_seconds &&
         left->modified_nanoseconds == right->modified_nanoseconds && left->mode == right->mode &&
         left->uid == right->uid;
}

static void mf_pt_path_snapshot_reset(mf_pt_path_snapshot* snapshot) {
  if (snapshot == NULL) {
    return;
  }
  free(snapshot->resolved_path);
  memset(snapshot, 0, sizeof(*snapshot));
}

static mf_cuda_passthrough_status_v1
mf_pt_path_snapshot_capture(const char* path, int allow_missing, mf_pt_path_snapshot* snapshot) {
  struct stat source_attributes;
  struct stat target_attributes;
  char* resolved = NULL;

  if (path == NULL || path[0] != '/' || snapshot == NULL) {
    return MF_CUDA_PASSTHROUGH_INVALID_ARGUMENT;
  }
  memset(snapshot, 0, sizeof(*snapshot));
  if (lstat(path, &source_attributes) != 0) {
    if (allow_missing != 0 && errno == ENOENT) {
      return MF_CUDA_PASSTHROUGH_SUCCESS;
    }
    return errno == ENOENT ? MF_CUDA_PASSTHROUGH_NOT_FOUND : MF_CUDA_PASSTHROUGH_SYSTEM_ERROR;
  }
  resolved = realpath(path, NULL);
  if (resolved == NULL) {
    return errno == ENOENT ? MF_CUDA_PASSTHROUGH_NOT_FOUND : MF_CUDA_PASSTHROUGH_SYSTEM_ERROR;
  }
  if (resolved[0] != '/') {
    free(resolved);
    return MF_CUDA_PASSTHROUGH_SYSTEM_ERROR;
  }
  if (stat(resolved, &target_attributes) != 0) {
    char* magic_link_path = NULL;
    if (stat(path, &target_attributes) != 0) {
      free(resolved);
      return MF_CUDA_PASSTHROUGH_SYSTEM_ERROR;
    }
    magic_link_path = strdup(path);
    if (magic_link_path == NULL) {
      free(resolved);
      return MF_CUDA_PASSTHROUGH_SYSTEM_ERROR;
    }
    free(resolved);
    resolved = magic_link_path;
  }
  snapshot->exists = UINT32_C(1);
  mf_pt_stat_from_native(&source_attributes, &snapshot->source);
  mf_pt_stat_from_native(&target_attributes, &snapshot->target);
  snapshot->resolved_path = resolved;
  return MF_CUDA_PASSTHROUGH_SUCCESS;
}

static int mf_pt_path_snapshot_equal(const mf_pt_path_snapshot* left,
                                     const mf_pt_path_snapshot* right) {
  if (left->exists != right->exists) {
    return 0;
  }
  if (left->exists == UINT32_C(0)) {
    return 1;
  }
  return left->resolved_path != NULL && right->resolved_path != NULL &&
         strcmp(left->resolved_path, right->resolved_path) == 0 &&
         mf_pt_stat_equal(&left->source, &right->source) != 0 &&
         mf_pt_stat_equal(&left->target, &right->target) != 0;
}

static mf_cuda_passthrough_status_v1
mf_pt_path_snapshot_is_current(const char* path, const mf_pt_path_snapshot* expected) {
  mf_pt_path_snapshot current;
  mf_cuda_passthrough_status_v1 status =
      mf_pt_path_snapshot_capture(path, expected->exists == UINT32_C(0), &current);
  if (status != MF_CUDA_PASSTHROUGH_SUCCESS) {
    return MF_CUDA_PASSTHROUGH_STALE;
  }
  status = mf_pt_path_snapshot_equal(expected, &current) != 0 ? MF_CUDA_PASSTHROUGH_SUCCESS
                                                              : MF_CUDA_PASSTHROUGH_STALE;
  mf_pt_path_snapshot_reset(&current);
  return status;
}

static int mf_pt_range_valid(uint64_t offset, uint64_t size, uint64_t file_size) {
  return offset <= file_size && size <= file_size - offset;
}

static int mf_pt_multiply_u64(uint64_t left, uint64_t right, uint64_t* out_value) {
  if (out_value == NULL || (left != UINT64_C(0) && right > UINT64_MAX / left)) {
    return 0;
  }
  *out_value = left * right;
  return 1;
}

static int mf_pt_pread_all(int fd, void* bytes, size_t byte_count, uint64_t offset) {
  uint8_t* output = (uint8_t*)bytes;
  size_t completed = 0;
  if (fd < 0 || bytes == NULL || offset > (uint64_t)INT64_MAX ||
      byte_count > (size_t)((uint64_t)INT64_MAX - offset)) {
    return 0;
  }
  while (completed < byte_count) {
    const ssize_t result =
        pread(fd, output + completed, byte_count - completed, (off_t)(offset + completed));
    if (result < 0 && errno == EINTR) {
      continue;
    }
    if (result <= 0) {
      return 0;
    }
    completed += (size_t)result;
  }
  return 1;
}

static uint64_t mf_pt_align4(uint64_t value, int* valid) {
  if (value > UINT64_MAX - UINT64_C(3)) {
    *valid = 0;
    return UINT64_C(0);
  }
  return (value + UINT64_C(3)) & ~UINT64_C(3);
}

static int mf_pt_virtual_to_file(const Elf64_Phdr* headers, uint32_t count, uint64_t address,
                                 uint64_t size, uint64_t file_size, uint64_t* out_offset) {
  uint32_t index = 0;
  for (index = 0; index < count; ++index) {
    const Elf64_Phdr* header = &headers[index];
    uint64_t delta = 0;
    if (header->p_type != PT_LOAD || address < header->p_vaddr) {
      continue;
    }
    delta = address - header->p_vaddr;
    if (delta <= header->p_filesz && size <= header->p_filesz - delta &&
        header->p_offset <= UINT64_MAX - delta &&
        mf_pt_range_valid(header->p_offset + delta, size, file_size) != 0) {
      *out_offset = header->p_offset + delta;
      return 1;
    }
  }
  return 0;
}

static mf_cuda_passthrough_status_v1 mf_pt_parse_notes(int fd, const Elf64_Phdr* headers,
                                                       uint32_t count, uint64_t file_size,
                                                       mf_pt_elf_identity* identity) {
  uint32_t program_index = 0;
  int found = 0;
  for (program_index = 0; program_index < count; ++program_index) {
    const Elf64_Phdr* program = &headers[program_index];
    uint64_t cursor = 0;
    if (program->p_type != PT_NOTE) {
      continue;
    }
    if (mf_pt_range_valid(program->p_offset, program->p_filesz, file_size) == 0) {
      return MF_CUDA_PASSTHROUGH_MALFORMED_ELF;
    }
    while (cursor < program->p_filesz) {
      Elf64_Nhdr note;
      uint64_t name_size = 0;
      uint64_t description_size = 0;
      uint64_t name_offset = 0;
      uint64_t description_offset = 0;
      uint64_t next = 0;
      char name[4];
      int valid = 1;
      if (program->p_filesz - cursor < sizeof(note) ||
          mf_pt_pread_all(fd, &note, sizeof(note), program->p_offset + cursor) == 0) {
        return MF_CUDA_PASSTHROUGH_MALFORMED_ELF;
      }
      name_size = mf_pt_align4(note.n_namesz, &valid);
      description_size = mf_pt_align4(note.n_descsz, &valid);
      if (valid == 0 || cursor > UINT64_MAX - sizeof(note)) {
        return MF_CUDA_PASSTHROUGH_MALFORMED_ELF;
      }
      name_offset = cursor + sizeof(note);
      if (name_offset > UINT64_MAX - name_size) {
        return MF_CUDA_PASSTHROUGH_MALFORMED_ELF;
      }
      description_offset = name_offset + name_size;
      if (description_offset > UINT64_MAX - description_size) {
        return MF_CUDA_PASSTHROUGH_MALFORMED_ELF;
      }
      next = description_offset + description_size;
      if (next <= cursor || next > program->p_filesz) {
        return MF_CUDA_PASSTHROUGH_MALFORMED_ELF;
      }
      if (note.n_type == NT_GNU_BUILD_ID && note.n_namesz == sizeof(name) &&
          note.n_descsz >= UINT32_C(4) && note.n_descsz <= MF_PT_MAX_BUILD_ID &&
          mf_pt_pread_all(fd, name, sizeof(name), program->p_offset + name_offset) != 0 &&
          memcmp(name, "GNU\0", sizeof(name)) == 0) {
        uint8_t current[MF_PT_MAX_BUILD_ID];
        if (mf_pt_pread_all(fd, current, note.n_descsz, program->p_offset + description_offset) ==
            0) {
          return MF_CUDA_PASSTHROUGH_MALFORMED_ELF;
        }
        if (found != 0 && (identity->build_id_size != note.n_descsz ||
                           memcmp(identity->build_id, current, note.n_descsz) != 0)) {
          return MF_CUDA_PASSTHROUGH_MALFORMED_ELF;
        }
        memcpy(identity->build_id, current, note.n_descsz);
        identity->build_id_size = note.n_descsz;
        found = 1;
      }
      cursor = next;
    }
  }
  return found != 0 ? MF_CUDA_PASSTHROUGH_SUCCESS : MF_CUDA_PASSTHROUGH_MALFORMED_ELF;
}

static mf_cuda_passthrough_status_v1 mf_pt_parse_elf(int fd, uint64_t file_size,
                                                     const char* expected_soname,
                                                     mf_pt_elf_identity* identity) {
  Elf64_Ehdr header;
  Elf64_Phdr* programs = NULL;
  uint64_t program_bytes = 0;
  uint64_t dynamic_offset = 0;
  uint64_t dynamic_size = 0;
  uint64_t string_address = 0;
  uint64_t string_size = 0;
  uint64_t soname_index = UINT64_MAX;
  uint64_t string_offset = 0;
  uint32_t program_index = 0;
  uint64_t dynamic_index = 0;
  int dynamic_found = 0;
  int dynamic_terminated = 0;
  mf_cuda_passthrough_status_v1 status = MF_CUDA_PASSTHROUGH_MALFORMED_ELF;

  if (fd < 0 || identity == NULL || file_size < sizeof(header) ||
      mf_pt_pread_all(fd, &header, sizeof(header), UINT64_C(0)) == 0) {
    return MF_CUDA_PASSTHROUGH_MALFORMED_ELF;
  }
  memset(identity, 0, sizeof(*identity));
  if (memcmp(header.e_ident, ELFMAG, SELFMAG) != 0 || header.e_ident[EI_CLASS] != ELFCLASS64 ||
      header.e_ident[EI_DATA] != ELFDATA2LSB || header.e_ident[EI_VERSION] != EV_CURRENT ||
      header.e_type != ET_DYN || header.e_machine != EM_X86_64 || header.e_version != EV_CURRENT ||
      header.e_ehsize != sizeof(Elf64_Ehdr) || header.e_phentsize != sizeof(Elf64_Phdr) ||
      header.e_phnum == 0U || header.e_phnum == PN_XNUM ||
      header.e_phnum > MF_PT_MAX_PROGRAM_HEADERS ||
      mf_pt_multiply_u64(header.e_phnum, sizeof(Elf64_Phdr), &program_bytes) == 0 ||
      mf_pt_range_valid(header.e_phoff, program_bytes, file_size) == 0 ||
      program_bytes > (uint64_t)SIZE_MAX) {
    return MF_CUDA_PASSTHROUGH_MALFORMED_ELF;
  }
  programs = (Elf64_Phdr*)malloc((size_t)program_bytes);
  if (programs == NULL) {
    return MF_CUDA_PASSTHROUGH_SYSTEM_ERROR;
  }
  if (mf_pt_pread_all(fd, programs, (size_t)program_bytes, header.e_phoff) == 0) {
    goto cleanup;
  }
  for (program_index = 0; program_index < header.e_phnum; ++program_index) {
    if (programs[program_index].p_type == PT_DYNAMIC) {
      if (dynamic_found != 0 ||
          mf_pt_range_valid(programs[program_index].p_offset, programs[program_index].p_filesz,
                            file_size) == 0 ||
          programs[program_index].p_filesz % sizeof(Elf64_Dyn) != UINT64_C(0)) {
        goto cleanup;
      }
      dynamic_found = 1;
      dynamic_offset = programs[program_index].p_offset;
      dynamic_size = programs[program_index].p_filesz;
    } else if (programs[program_index].p_type == PT_LOAD &&
               mf_pt_range_valid(programs[program_index].p_offset, programs[program_index].p_filesz,
                                 file_size) == 0) {
      goto cleanup;
    }
  }
  if (dynamic_size == UINT64_C(0) || dynamic_size / sizeof(Elf64_Dyn) > MF_PT_MAX_DYNAMIC_ENTRIES) {
    goto cleanup;
  }
  for (dynamic_index = 0; dynamic_index < dynamic_size / sizeof(Elf64_Dyn); ++dynamic_index) {
    Elf64_Dyn entry;
    if (mf_pt_pread_all(fd, &entry, sizeof(entry),
                        dynamic_offset + dynamic_index * sizeof(entry)) == 0) {
      goto cleanup;
    }
    if (entry.d_tag == DT_NULL) {
      dynamic_terminated = 1;
      break;
    }
    if (entry.d_tag == DT_STRTAB) {
      if (string_address != UINT64_C(0) && string_address != entry.d_un.d_ptr) {
        goto cleanup;
      }
      string_address = entry.d_un.d_ptr;
    } else if (entry.d_tag == DT_STRSZ) {
      if (string_size != UINT64_C(0) && string_size != entry.d_un.d_val) {
        goto cleanup;
      }
      string_size = entry.d_un.d_val;
    } else if (entry.d_tag == DT_SONAME) {
      if (soname_index != UINT64_MAX && soname_index != entry.d_un.d_val) {
        goto cleanup;
      }
      soname_index = entry.d_un.d_val;
    }
  }
  if (dynamic_terminated == 0 || string_address == UINT64_C(0) || string_size == UINT64_C(0) ||
      soname_index == UINT64_MAX || soname_index >= string_size ||
      mf_pt_virtual_to_file(programs, header.e_phnum, string_address, string_size, file_size,
                            &string_offset) == 0) {
    goto cleanup;
  }
  {
    uint64_t available = string_size - soname_index;
    uint64_t index = 0;
    const uint64_t limit =
        available < MF_PT_MAX_SONAME + UINT64_C(1) ? available : MF_PT_MAX_SONAME + UINT64_C(1);
    int terminated = 0;
    for (index = 0; index < limit; ++index) {
      if (mf_pt_pread_all(fd, &identity->soname[index], sizeof(char),
                          string_offset + soname_index + index) == 0) {
        goto cleanup;
      }
      if (identity->soname[index] == '\0') {
        terminated = 1;
        break;
      }
    }
    if (terminated == 0 || identity->soname[0] == '\0' ||
        (expected_soname != NULL && strcmp(identity->soname, expected_soname) != 0)) {
      goto cleanup;
    }
  }
  status = mf_pt_parse_notes(fd, programs, header.e_phnum, file_size, identity);

cleanup:
  free(programs);
  return status;
}

static int mf_pt_path_under_root(const char* path, const char* root) {
  size_t root_length = 0;
  if (path == NULL || root == NULL || path[0] != '/' || root[0] != '/') {
    return 0;
  }
  root_length = strlen(root);
  while (root_length > (size_t)1 && root[root_length - (size_t)1] == '/') {
    --root_length;
  }
  if (strncmp(path, root, root_length) != 0) {
    return 0;
  }
  return path[root_length] == '\0' || path[root_length] == '/';
}

static int mf_pt_path_under_install_root(const char* path, const char* install_root) {
  char* resolved_root = NULL;
  int result = mf_pt_path_under_root(path, install_root);
  if (result != 0) {
    return result;
  }
  resolved_root = realpath(install_root, NULL);
  if (resolved_root != NULL) {
    result = mf_pt_path_under_root(path, resolved_root);
  }
  free(resolved_root);
  return result;
}

static mf_cuda_passthrough_status_v1 mf_pt_validate_ancestors(const char* resolved_path,
                                                              uint32_t trusted_uid) {
  char* copy = NULL;
  char* cursor = NULL;
  char* final_slash = NULL;
  mf_cuda_passthrough_status_v1 status = MF_CUDA_PASSTHROUGH_SUCCESS;
  if (resolved_path == NULL || resolved_path[0] != '/') {
    return MF_CUDA_PASSTHROUGH_INVALID_ARGUMENT;
  }
  copy = strdup(resolved_path);
  if (copy == NULL) {
    return MF_CUDA_PASSTHROUGH_SYSTEM_ERROR;
  }
  final_slash = strrchr(copy, '/');
  if (final_slash == NULL) {
    free(copy);
    return MF_CUDA_PASSTHROUGH_POLICY_REJECTED;
  }
  if (final_slash == copy) {
    final_slash[1] = '\0';
  } else {
    *final_slash = '\0';
  }
  cursor = copy + (size_t)1;
  for (;;) {
    char* slash = strchr(cursor, '/');
    struct stat attributes;
    if (slash != NULL) {
      *slash = '\0';
    }
    if (stat(copy, &attributes) != 0 || !S_ISDIR(attributes.st_mode) ||
        (uint32_t)attributes.st_uid != trusted_uid ||
        (attributes.st_mode & (S_IWGRP | S_IWOTH)) != 0) {
      status = MF_CUDA_PASSTHROUGH_POLICY_REJECTED;
      break;
    }
    if (slash == NULL) {
      break;
    }
    *slash = '/';
    cursor = slash + (size_t)1;
  }
  free(copy);
  return status;
}

static mf_cuda_passthrough_status_v1 mf_pt_validate_regular_file(const struct stat* attributes,
                                                                 uint32_t trusted_uid) {
  if (!S_ISREG(attributes->st_mode) || (uint32_t)attributes->st_uid != trusted_uid ||
      (attributes->st_mode & (S_IWGRP | S_IWOTH)) != 0) {
    return MF_CUDA_PASSTHROUGH_POLICY_REJECTED;
  }
  return MF_CUDA_PASSTHROUGH_SUCCESS;
}

static mf_cuda_passthrough_status_v1
mf_pt_open_snapshot_file(const mf_cuda_passthrough_policy_v1* policy, const char* path,
                         int allow_missing, mf_pt_path_snapshot* snapshot, int* out_fd) {
  struct stat attributes;
  mf_cuda_passthrough_status_v1 status = MF_CUDA_PASSTHROUGH_SUCCESS;
  int fd = -1;
  if (policy == NULL || snapshot == NULL || out_fd == NULL) {
    return MF_CUDA_PASSTHROUGH_INVALID_ARGUMENT;
  }
  *out_fd = -1;
  status = mf_pt_path_snapshot_capture(path, allow_missing, snapshot);
  if (status != MF_CUDA_PASSTHROUGH_SUCCESS || snapshot->exists == UINT32_C(0)) {
    return status;
  }
  if (policy->enforce_trusted_ancestors != UINT32_C(0)) {
    status = mf_pt_validate_ancestors(snapshot->resolved_path, policy->trusted_uid);
    if (status != MF_CUDA_PASSTHROUGH_SUCCESS) {
      return status;
    }
  }
  fd = open(snapshot->resolved_path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
  if (fd < 0 || fstat(fd, &attributes) != 0) {
    if (fd >= 0) {
      (void)close(fd);
    }
    return MF_CUDA_PASSTHROUGH_SYSTEM_ERROR;
  }
  status = mf_pt_validate_regular_file(&attributes, policy->trusted_uid);
  if (status != MF_CUDA_PASSTHROUGH_SUCCESS ||
      (uint64_t)attributes.st_dev != snapshot->target.device ||
      (uint64_t)attributes.st_ino != snapshot->target.inode) {
    (void)close(fd);
    return status == MF_CUDA_PASSTHROUGH_SUCCESS ? MF_CUDA_PASSTHROUGH_STALE : status;
  }
  *out_fd = fd;
  return MF_CUDA_PASSTHROUGH_SUCCESS;
}

static int mf_pt_build_text_valid(const char* text, size_t length) {
  size_t index = 0;
  uint32_t dots = 0;
  if (text == NULL || length == (size_t)0 || length > MF_PT_MAX_BUILD_TEXT || text[0] < '0' ||
      text[0] > '9' || text[length - (size_t)1] < '0' || text[length - (size_t)1] > '9') {
    return 0;
  }
  for (index = 0; index < length; ++index) {
    if (text[index] == '.') {
      if (index == (size_t)0 || index + (size_t)1 >= length || text[index - (size_t)1] == '.' ||
          text[index + (size_t)1] == '.') {
        return 0;
      }
      ++dots;
    } else if (text[index] < '0' || text[index] > '9') {
      return 0;
    }
  }
  return dots >= UINT32_C(2);
}

static mf_cuda_passthrough_status_v1
mf_pt_extract_filename_build(const char* path, const char* prefix,
                             char out_build[MF_PT_MAX_BUILD_TEXT + UINT32_C(1)]) {
  const char* basename = NULL;
  const char* suffix = NULL;
  size_t prefix_length = 0;
  size_t suffix_length = 0;
  if (path == NULL || prefix == NULL || out_build == NULL) {
    return MF_CUDA_PASSTHROUGH_INVALID_ARGUMENT;
  }
  basename = strrchr(path, '/');
  basename = basename == NULL ? path : basename + (size_t)1;
  prefix_length = strlen(prefix);
  if (strncmp(basename, prefix, prefix_length) != 0) {
    return MF_CUDA_PASSTHROUGH_BUILD_MISMATCH;
  }
  suffix = basename + prefix_length;
  suffix_length = strlen(suffix);
  if (mf_pt_build_text_valid(suffix, suffix_length) == 0) {
    return MF_CUDA_PASSTHROUGH_BUILD_MISMATCH;
  }
  memcpy(out_build, suffix, suffix_length + (size_t)1);
  return MF_CUDA_PASSTHROUGH_SUCCESS;
}

static mf_cuda_passthrough_status_v1 mf_pt_read_limited(int fd, uint8_t* output, size_t capacity,
                                                        size_t* out_size) {
  size_t used = 0;
  if (fd < 0 || output == NULL || capacity == (size_t)0 || out_size == NULL) {
    return MF_CUDA_PASSTHROUGH_INVALID_ARGUMENT;
  }
  while (used < capacity) {
    const ssize_t result = pread(fd, output + used, capacity - used, (off_t)used);
    if (result < 0 && errno == EINTR) {
      continue;
    }
    if (result < 0) {
      return MF_CUDA_PASSTHROUGH_SYSTEM_ERROR;
    }
    if (result == 0) {
      *out_size = used;
      return MF_CUDA_PASSTHROUGH_SUCCESS;
    }
    used += (size_t)result;
  }
  {
    uint8_t extra = UINT8_C(0);
    ssize_t result = -1;
    do {
      result = pread(fd, &extra, sizeof(extra), (off_t)capacity);
    } while (result < 0 && errno == EINTR);
    if (result < 0) {
      return MF_CUDA_PASSTHROUGH_SYSTEM_ERROR;
    }
    if (result != 0) {
      return MF_CUDA_PASSTHROUGH_POLICY_REJECTED;
    }
  }
  *out_size = used;
  return MF_CUDA_PASSTHROUGH_SUCCESS;
}

static mf_cuda_passthrough_status_v1
mf_pt_extract_proc_build(const uint8_t* bytes, size_t byte_count,
                         char out_build[MF_PT_MAX_BUILD_TEXT + UINT32_C(1)]) {
  size_t line_end = 0;
  size_t index = 0;
  if (bytes == NULL || byte_count == (size_t)0 || out_build == NULL) {
    return MF_CUDA_PASSTHROUGH_BUILD_MISMATCH;
  }
  while (line_end < byte_count && bytes[line_end] != '\n' && bytes[line_end] != '\0') {
    ++line_end;
  }
  for (index = 0; index < line_end; ++index) {
    size_t end = index;
    if (bytes[index] < '0' || bytes[index] > '9') {
      continue;
    }
    while (end < line_end && ((bytes[end] >= '0' && bytes[end] <= '9') || bytes[end] == '.')) {
      ++end;
    }
    if (mf_pt_build_text_valid((const char*)bytes + index, end - index) != 0) {
      memcpy(out_build, bytes + index, end - index);
      out_build[end - index] = '\0';
      return MF_CUDA_PASSTHROUGH_SUCCESS;
    }
    index = end;
  }
  return MF_CUDA_PASSTHROUGH_BUILD_MISMATCH;
}

static int mf_pt_build_id_equal(const mf_pt_elf_identity* left, const mf_pt_elf_identity* right) {
  return left->build_id_size != UINT32_C(0) && left->build_id_size == right->build_id_size &&
         memcmp(left->build_id, right->build_id, left->build_id_size) == 0;
}

static void mf_pt_library_record_reset(mf_pt_library_record* record) {
  if (record == NULL) {
    return;
  }
  if (record->fd >= 0) {
    (void)close(record->fd);
  }
  free(record->source_path);
  mf_pt_path_snapshot_reset(&record->path);
  memset(record, 0, sizeof(*record));
  record->fd = -1;
}

static mf_cuda_passthrough_status_v1 mf_pt_open_library(const mf_cuda_passthrough_policy_v1* policy,
                                                        const char* path,
                                                        const char* expected_soname,
                                                        const char* filename_prefix,
                                                        mf_pt_library_record* record) {
  struct stat attributes;
  mf_cuda_passthrough_status_v1 status = MF_CUDA_PASSTHROUGH_SUCCESS;
  if (record == NULL) {
    return MF_CUDA_PASSTHROUGH_INVALID_ARGUMENT;
  }
  memset(record, 0, sizeof(*record));
  record->fd = -1;
  record->source_path = strdup(path);
  if (record->source_path == NULL) {
    return MF_CUDA_PASSTHROUGH_SYSTEM_ERROR;
  }
  status = mf_pt_open_snapshot_file(policy, path, 0, &record->path, &record->fd);
  if (status != MF_CUDA_PASSTHROUGH_SUCCESS) {
    return status;
  }
  if (mf_pt_path_under_install_root(record->path.resolved_path, policy->install_root) != 0) {
    return MF_CUDA_PASSTHROUGH_POLICY_REJECTED;
  }
  if (fstat(record->fd, &attributes) != 0 || attributes.st_size <= 0) {
    return MF_CUDA_PASSTHROUGH_SYSTEM_ERROR;
  }
  status = mf_pt_parse_elf(record->fd, (uint64_t)attributes.st_size, expected_soname, &record->elf);
  if (status != MF_CUDA_PASSTHROUGH_SUCCESS) {
    return status;
  }
  return mf_pt_extract_filename_build(record->path.resolved_path, filename_prefix,
                                      record->driver_build);
}

static mf_cuda_passthrough_status_v1
mf_pt_read_proc_version(const mf_cuda_passthrough_policy_v1* policy, mf_pt_path_snapshot* snapshot,
                        char out_build[MF_PT_MAX_BUILD_TEXT + UINT32_C(1)]) {
  uint8_t bytes[MF_PT_MAX_PROC_VERSION_SIZE];
  size_t byte_count = 0;
  int fd = -1;
  mf_cuda_passthrough_status_v1 status =
      mf_pt_open_snapshot_file(policy, policy->proc_version_path, 0, snapshot, &fd);
  if (status == MF_CUDA_PASSTHROUGH_SUCCESS) {
    status = mf_pt_read_limited(fd, bytes, sizeof(bytes), &byte_count);
  }
  if (fd >= 0) {
    (void)close(fd);
  }
  if (status != MF_CUDA_PASSTHROUGH_SUCCESS) {
    return status;
  }
  return mf_pt_extract_proc_build(bytes, byte_count, out_build);
}

static mf_cuda_passthrough_status_v1
mf_pt_read_identity(const mf_cuda_passthrough_policy_v1* policy, const char* path,
                    mf_pt_path_snapshot* snapshot, mf_pt_elf_identity* identity) {
  struct stat attributes;
  int fd = -1;
  mf_cuda_passthrough_status_v1 status = mf_pt_open_snapshot_file(policy, path, 0, snapshot, &fd);
  if (status == MF_CUDA_PASSTHROUGH_SUCCESS &&
      (fstat(fd, &attributes) != 0 || attributes.st_size <= 0)) {
    status = MF_CUDA_PASSTHROUGH_SYSTEM_ERROR;
  }
  if (status == MF_CUDA_PASSTHROUGH_SUCCESS) {
    status = mf_pt_parse_elf(fd, (uint64_t)attributes.st_size, NULL, identity);
  }
  if (fd >= 0) {
    (void)close(fd);
  }
  return status;
}

static mf_cuda_passthrough_status_v1 mf_pt_parse_config(const mf_cuda_passthrough_policy_v1* policy,
                                                        mf_pt_path_snapshot* snapshot,
                                                        char** out_cuda_path,
                                                        char** out_nvml_path) {
  uint8_t bytes[MF_PT_MAX_CONFIG_SIZE + UINT32_C(1)];
  size_t byte_count = 0;
  size_t cursor = 0;
  int fd = -1;
  mf_cuda_passthrough_status_v1 status = MF_CUDA_PASSTHROUGH_SUCCESS;
  char* cuda_path = NULL;
  char* nvml_path = NULL;

  *out_cuda_path = NULL;
  *out_nvml_path = NULL;
  status = mf_pt_open_snapshot_file(policy, policy->config_path, 1, snapshot, &fd);
  if (status != MF_CUDA_PASSTHROUGH_SUCCESS || snapshot->exists == UINT32_C(0)) {
    return status;
  }
  status = mf_pt_read_limited(fd, bytes, MF_PT_MAX_CONFIG_SIZE, &byte_count);
  (void)close(fd);
  if (status != MF_CUDA_PASSTHROUGH_SUCCESS || byte_count == (size_t)0) {
    return status == MF_CUDA_PASSTHROUGH_SUCCESS ? MF_CUDA_PASSTHROUGH_MALFORMED_CONFIG : status;
  }
  bytes[byte_count] = UINT8_C(0);
  while (cursor < byte_count) {
    size_t end = cursor;
    const char* value = NULL;
    char** destination = NULL;
    while (end < byte_count && bytes[end] != '\n') {
      if (bytes[end] == '\0' || bytes[end] == '\r') {
        status = MF_CUDA_PASSTHROUGH_MALFORMED_CONFIG;
        goto cleanup;
      }
      ++end;
    }
    if (end == cursor) {
      status = MF_CUDA_PASSTHROUGH_MALFORMED_CONFIG;
      goto cleanup;
    }
    bytes[end] = UINT8_C(0);
    if (strncmp((const char*)bytes + cursor, "cuda=", (size_t)5) == 0) {
      destination = &cuda_path;
      value = (const char*)bytes + cursor + (size_t)5;
    } else if (strncmp((const char*)bytes + cursor, "nvml=", (size_t)5) == 0) {
      destination = &nvml_path;
      value = (const char*)bytes + cursor + (size_t)5;
    } else {
      status = MF_CUDA_PASSTHROUGH_MALFORMED_CONFIG;
      goto cleanup;
    }
    if (*destination != NULL || value[0] != '/') {
      status = MF_CUDA_PASSTHROUGH_MALFORMED_CONFIG;
      goto cleanup;
    }
    *destination = strdup(value);
    if (*destination == NULL) {
      status = MF_CUDA_PASSTHROUGH_SYSTEM_ERROR;
      goto cleanup;
    }
    cursor = end + (size_t)1;
  }
  if (cuda_path == NULL || nvml_path == NULL) {
    status = MF_CUDA_PASSTHROUGH_MALFORMED_CONFIG;
    goto cleanup;
  }
  *out_cuda_path = cuda_path;
  *out_nvml_path = nvml_path;
  return MF_CUDA_PASSTHROUGH_SUCCESS;

cleanup:
  free(cuda_path);
  free(nvml_path);
  return status;
}

static int mf_pt_policy_valid(const mf_cuda_passthrough_policy_v1* policy) {
  uint32_t index = 0;
  if (policy == NULL || policy->struct_size != sizeof(*policy) ||
      policy->abi_version != MF_CUDA_PASSTHROUGH_ABI_VERSION_V1 || policy->config_path == NULL ||
      policy->config_path[0] != '/' || policy->proc_version_path == NULL ||
      policy->proc_version_path[0] != '/' || policy->pid_namespace_path == NULL ||
      policy->pid_namespace_path[0] != '/' || policy->mount_namespace_path == NULL ||
      policy->mount_namespace_path[0] != '/' || policy->install_root == NULL ||
      policy->install_root[0] != '/' || policy->provider_self_path == NULL ||
      policy->provider_self_path[0] != '/' || policy->default_pair_count == UINT32_C(0) ||
      policy->default_pair_count > MF_CUDA_PASSTHROUGH_MAX_DEFAULT_PAIRS ||
      policy->reserved != UINT32_C(0)) {
    return 0;
  }
  for (index = 0; index < policy->default_pair_count; ++index) {
    if (policy->default_cuda_paths[index] == NULL || policy->default_cuda_paths[index][0] != '/' ||
        policy->default_nvml_paths[index] == NULL || policy->default_nvml_paths[index][0] != '/') {
      return 0;
    }
  }
  return 1;
}

void mf_cuda_passthrough_test_policy_init_v1(mf_cuda_passthrough_policy_v1* policy) {
  uint32_t index = 0;
  if (policy == NULL) {
    return;
  }
  memset(policy, 0, sizeof(*policy));
  policy->struct_size = sizeof(*policy);
  policy->abi_version = MF_CUDA_PASSTHROUGH_ABI_VERSION_V1;
  policy->trusted_uid = UINT32_C(0);
  policy->enforce_trusted_ancestors = UINT32_C(1);
  policy->config_path = "/etc/metaflux/vendor-libraries.conf";
  policy->proc_version_path = "/proc/driver/nvidia/version";
  policy->pid_namespace_path = "/proc/self/ns/pid";
  policy->mount_namespace_path = "/proc/self/ns/mnt";
  policy->install_root = MF_CUDA_PASSTHROUGH_INSTALL_ROOT;
  policy->default_pair_count = MF_CUDA_PASSTHROUGH_MAX_DEFAULT_PAIRS;
  for (index = 0; index < MF_CUDA_PASSTHROUGH_MAX_DEFAULT_PAIRS; ++index) {
    policy->default_cuda_paths[index] = mf_pt_default_cuda_paths[index];
    policy->default_nvml_paths[index] = mf_pt_default_nvml_paths[index];
  }
}

static void mf_pt_pair_destroy(mf_cuda_passthrough_pair_v1* pair) {
  if (pair == NULL) {
    return;
  }
  if (pair->nvml_handle != NULL) {
    (void)dlclose(pair->nvml_handle);
  }
  if (pair->cuda_handle != NULL) {
    (void)dlclose(pair->cuda_handle);
  }
  mf_pt_library_record_reset(&pair->nvml);
  mf_pt_library_record_reset(&pair->cuda);
  mf_pt_path_snapshot_reset(&pair->mount_namespace);
  mf_pt_path_snapshot_reset(&pair->pid_namespace);
  mf_pt_path_snapshot_reset(&pair->proc_version);
  mf_pt_path_snapshot_reset(&pair->config);
  free(pair->mount_namespace_path);
  free(pair->pid_namespace_path);
  free(pair->proc_version_path);
  free(pair->config_path);
  memset(pair, 0, sizeof(*pair));
  free(pair);
}

static mf_cuda_passthrough_status_v1 mf_pt_lookup_required(void* handle, const char* name,
                                                           const char* version, void** out_symbol) {
  const char* error = NULL;
  void* symbol = NULL;
  if (handle == NULL || name == NULL || name[0] == '\0' || version == NULL || version[0] == '\0' ||
      out_symbol == NULL) {
    return MF_CUDA_PASSTHROUGH_INVALID_ARGUMENT;
  }
  (void)dlerror();
  symbol = dlvsym(handle, name, version);
  error = dlerror();
  if (error != NULL || symbol == NULL) {
    return MF_CUDA_PASSTHROUGH_SYMBOL_MISSING;
  }
  *out_symbol = symbol;
  return MF_CUDA_PASSTHROUGH_SUCCESS;
}

static mf_cuda_passthrough_status_v1 mf_pt_load_namespace(mf_cuda_passthrough_pair_v1* pair) {
  mf_cuda_passthrough_status_v1 status = MF_CUDA_PASSTHROUGH_SUCCESS;
  const int flags = RTLD_NOW | RTLD_LOCAL;
  Lmid_t nvml_namespace = LM_ID_BASE;
  if (pair == NULL || pair->cuda.path.resolved_path == NULL ||
      pair->nvml.path.resolved_path == NULL) {
    return MF_CUDA_PASSTHROUGH_INVALID_ARGUMENT;
  }
  (void)dlerror();
  pair->cuda_handle = dlmopen(LM_ID_NEWLM, pair->cuda.path.resolved_path, flags);
  if (pair->cuda_handle == NULL || dlerror() != NULL ||
      dlinfo(pair->cuda_handle, RTLD_DI_LMID, &pair->namespace_id) != 0 ||
      pair->namespace_id == LM_ID_BASE || pair->namespace_id == LM_ID_NEWLM) {
    return MF_CUDA_PASSTHROUGH_LOAD_FAILED;
  }
  (void)dlerror();
  pair->nvml_handle = dlmopen(pair->namespace_id, pair->nvml.path.resolved_path, flags);
  if (pair->nvml_handle == NULL || dlerror() != NULL ||
      dlinfo(pair->nvml_handle, RTLD_DI_LMID, &nvml_namespace) != 0 ||
      nvml_namespace != pair->namespace_id) {
    return MF_CUDA_PASSTHROUGH_LOAD_FAILED;
  }
  pair->bootstrap.struct_size = sizeof(pair->bootstrap);
  pair->bootstrap.abi_version = MF_CUDA_PASSTHROUGH_ABI_VERSION_V1;
  status =
      mf_pt_lookup_required(pair->cuda_handle, "cuInit", "libcuda.so.1", &pair->bootstrap.cu_init);
  if (status == MF_CUDA_PASSTHROUGH_SUCCESS) {
    status = mf_pt_lookup_required(pair->cuda_handle, "cuDriverGetVersion", "libcuda.so.1",
                                   &pair->bootstrap.cu_driver_get_version);
  }
  if (status == MF_CUDA_PASSTHROUGH_SUCCESS) {
    status = mf_pt_lookup_required(pair->cuda_handle, "cuGetProcAddress", "libcuda.so.1",
                                   &pair->bootstrap.cu_get_proc_address);
  }
  if (status == MF_CUDA_PASSTHROUGH_SUCCESS) {
    status = mf_pt_lookup_required(pair->nvml_handle, "nvmlInit_v2", "libnvidia-ml.so.1",
                                   &pair->bootstrap.nvml_init_v2);
  }
  if (status == MF_CUDA_PASSTHROUGH_SUCCESS) {
    status = mf_pt_lookup_required(pair->nvml_handle, "nvmlShutdown", "libnvidia-ml.so.1",
                                   &pair->bootstrap.nvml_shutdown);
  }
  if (status == MF_CUDA_PASSTHROUGH_SUCCESS) {
    status =
        mf_pt_lookup_required(pair->nvml_handle, "nvmlSystemGetDriverVersion", "libnvidia-ml.so.1",
                              &pair->bootstrap.nvml_system_get_driver_version);
  }
  return status;
}

static mf_cuda_passthrough_status_v1
mf_pt_candidate_pair(const mf_cuda_passthrough_policy_v1* policy, const char* cuda_path,
                     const char* nvml_path, const mf_pt_path_snapshot* provider_path,
                     const mf_pt_elf_identity* provider_identity,
                     mf_cuda_passthrough_pair_v1* pair) {
  mf_cuda_passthrough_status_v1 status = MF_CUDA_PASSTHROUGH_SUCCESS;
  mf_pt_library_record_reset(&pair->cuda);
  mf_pt_library_record_reset(&pair->nvml);
  status = mf_pt_open_library(policy, cuda_path, "libcuda.so.1", "libcuda.so.", &pair->cuda);
  if (status == MF_CUDA_PASSTHROUGH_SUCCESS) {
    status =
        mf_pt_open_library(policy, nvml_path, "libnvidia-ml.so.1", "libnvidia-ml.so.", &pair->nvml);
  }
  if (status != MF_CUDA_PASSTHROUGH_SUCCESS) {
    return status;
  }
  if (strcmp(pair->cuda.driver_build, pair->nvml.driver_build) != 0 ||
      strcmp(pair->cuda.driver_build, pair->proc_driver_build) != 0) {
    return MF_CUDA_PASSTHROUGH_BUILD_MISMATCH;
  }
  if ((pair->cuda.path.target.device == provider_path->target.device &&
       pair->cuda.path.target.inode == provider_path->target.inode) ||
      (pair->nvml.path.target.device == provider_path->target.device &&
       pair->nvml.path.target.inode == provider_path->target.inode) ||
      mf_pt_build_id_equal(&pair->cuda.elf, provider_identity) != 0 ||
      mf_pt_build_id_equal(&pair->nvml.elf, provider_identity) != 0) {
    return MF_CUDA_PASSTHROUGH_POLICY_REJECTED;
  }
  status = mf_pt_load_namespace(pair);
  if (status != MF_CUDA_PASSTHROUGH_SUCCESS) {
    return status;
  }
  if (mf_pt_path_snapshot_is_current(pair->cuda.source_path, &pair->cuda.path) !=
          MF_CUDA_PASSTHROUGH_SUCCESS ||
      mf_pt_path_snapshot_is_current(pair->nvml.source_path, &pair->nvml.path) !=
          MF_CUDA_PASSTHROUGH_SUCCESS) {
    return MF_CUDA_PASSTHROUGH_STALE;
  }
  (void)close(pair->cuda.fd);
  pair->cuda.fd = -1;
  (void)close(pair->nvml.fd);
  pair->nvml.fd = -1;
  return MF_CUDA_PASSTHROUGH_SUCCESS;
}

mf_cuda_passthrough_status_v1
mf_cuda_passthrough_pair_load_with_policy_v1(const mf_cuda_passthrough_policy_v1* policy,
                                             mf_cuda_passthrough_pair_v1** out_pair) {
  mf_cuda_passthrough_pair_v1* pair = NULL;
  mf_pt_path_snapshot provider_path;
  mf_pt_elf_identity provider_identity;
  mf_cuda_passthrough_status_v1 status = MF_CUDA_PASSTHROUGH_SUCCESS;
  char* configured_cuda = NULL;
  char* configured_nvml = NULL;
  uint32_t index = 0;
  int selected = 0;

  if (out_pair == NULL) {
    return MF_CUDA_PASSTHROUGH_INVALID_ARGUMENT;
  }
  *out_pair = NULL;
  if (mf_pt_policy_valid(policy) == 0) {
    return MF_CUDA_PASSTHROUGH_INVALID_ARGUMENT;
  }
  memset(&provider_path, 0, sizeof(provider_path));
  memset(&provider_identity, 0, sizeof(provider_identity));
  pair = (mf_cuda_passthrough_pair_v1*)calloc((size_t)1, sizeof(*pair));
  if (pair == NULL) {
    return MF_CUDA_PASSTHROUGH_SYSTEM_ERROR;
  }
  pair->cuda.fd = -1;
  pair->nvml.fd = -1;
  pair->owner_pid = getpid();
  pair->trusted_uid = policy->trusted_uid;
  pair->enforce_trusted_ancestors = policy->enforce_trusted_ancestors;
  pair->namespace_id = LM_ID_BASE;
  pair->config_path = strdup(policy->config_path);
  pair->proc_version_path = strdup(policy->proc_version_path);
  pair->pid_namespace_path = strdup(policy->pid_namespace_path);
  pair->mount_namespace_path = strdup(policy->mount_namespace_path);
  if (pair->config_path == NULL || pair->proc_version_path == NULL ||
      pair->pid_namespace_path == NULL || pair->mount_namespace_path == NULL) {
    status = MF_CUDA_PASSTHROUGH_SYSTEM_ERROR;
    goto cleanup;
  }

  status = mf_pt_path_snapshot_capture(policy->pid_namespace_path, 0, &pair->pid_namespace);
  if (status == MF_CUDA_PASSTHROUGH_SUCCESS) {
    status = mf_pt_path_snapshot_capture(policy->mount_namespace_path, 0, &pair->mount_namespace);
  }
  if (status == MF_CUDA_PASSTHROUGH_SUCCESS) {
    status = mf_pt_read_proc_version(policy, &pair->proc_version, pair->proc_driver_build);
  }
  if (status == MF_CUDA_PASSTHROUGH_SUCCESS) {
    status = mf_pt_parse_config(policy, &pair->config, &configured_cuda, &configured_nvml);
  }
  if (status == MF_CUDA_PASSTHROUGH_SUCCESS) {
    status =
        mf_pt_read_identity(policy, policy->provider_self_path, &provider_path, &provider_identity);
  }
  if (status != MF_CUDA_PASSTHROUGH_SUCCESS) {
    goto cleanup;
  }

  if (pair->config.exists != UINT32_C(0)) {
    status = mf_pt_candidate_pair(policy, configured_cuda, configured_nvml, &provider_path,
                                  &provider_identity, pair);
    selected = status == MF_CUDA_PASSTHROUGH_SUCCESS;
  } else {
    status = MF_CUDA_PASSTHROUGH_NOT_FOUND;
    for (index = 0; index < policy->default_pair_count; ++index) {
      struct stat cuda_attributes;
      struct stat nvml_attributes;
      const int cuda_exists = lstat(policy->default_cuda_paths[index], &cuda_attributes) == 0;
      const int nvml_exists = lstat(policy->default_nvml_paths[index], &nvml_attributes) == 0;
      if (cuda_exists == 0 && nvml_exists == 0) {
        continue;
      }
      status = mf_pt_candidate_pair(policy, policy->default_cuda_paths[index],
                                    policy->default_nvml_paths[index], &provider_path,
                                    &provider_identity, pair);
      selected = status == MF_CUDA_PASSTHROUGH_SUCCESS;
      break;
    }
  }
  if (selected == 0) {
    goto cleanup;
  }
  if (mf_cuda_passthrough_pair_validate_current_v1(pair) != MF_CUDA_PASSTHROUGH_SUCCESS) {
    status = MF_CUDA_PASSTHROUGH_STALE;
    goto cleanup;
  }
  mf_pt_path_snapshot_reset(&provider_path);
  free(configured_nvml);
  free(configured_cuda);
  *out_pair = pair;
  return MF_CUDA_PASSTHROUGH_SUCCESS;

cleanup:
  mf_pt_path_snapshot_reset(&provider_path);
  free(configured_nvml);
  free(configured_cuda);
  mf_pt_pair_destroy(pair);
  return status;
}

static mf_cuda_passthrough_status_v1 mf_pt_production_policy(mf_cuda_passthrough_policy_v1* policy,
                                                             char** out_self_path) {
  Dl_info information;
  void (*marker)(mf_cuda_passthrough_policy_v1*) = mf_cuda_passthrough_test_policy_init_v1;
  void* address = NULL;
  char* resolved = NULL;
  _Static_assert(sizeof(address) == sizeof(marker), "POSIX function and data pointers must match");
  if (policy == NULL || out_self_path == NULL) {
    return MF_CUDA_PASSTHROUGH_INVALID_ARGUMENT;
  }
  *out_self_path = NULL;
  memset(&information, 0, sizeof(information));
  memcpy(&address, &marker, sizeof(address));
  if (dladdr(address, &information) == 0 || information.dli_fname == NULL) {
    return MF_CUDA_PASSTHROUGH_SYSTEM_ERROR;
  }
  resolved = realpath(information.dli_fname, NULL);
  if (resolved == NULL || resolved[0] != '/') {
    free(resolved);
    return MF_CUDA_PASSTHROUGH_SYSTEM_ERROR;
  }
  mf_cuda_passthrough_test_policy_init_v1(policy);
  policy->provider_self_path = resolved;
  *out_self_path = resolved;
  return MF_CUDA_PASSTHROUGH_SUCCESS;
}

mf_cuda_passthrough_status_v1
mf_cuda_passthrough_pair_load_v1(mf_cuda_passthrough_pair_v1** out_pair) {
  mf_cuda_passthrough_policy_v1 policy;
  char* self_path = NULL;
  mf_cuda_passthrough_status_v1 status = MF_CUDA_PASSTHROUGH_INVALID_ARGUMENT;
  if (out_pair == NULL) {
    return MF_CUDA_PASSTHROUGH_INVALID_ARGUMENT;
  }
  *out_pair = NULL;
  status = mf_pt_production_policy(&policy, &self_path);
  if (status == MF_CUDA_PASSTHROUGH_SUCCESS) {
    status = mf_cuda_passthrough_pair_load_with_policy_v1(&policy, out_pair);
  }
  free(self_path);
  return status;
}

void mf_cuda_passthrough_pair_release_v1(mf_cuda_passthrough_pair_v1* pair) {
  mf_pt_pair_destroy(pair);
}

static mf_cuda_passthrough_status_v1
mf_pt_validate_proc_version(const mf_cuda_passthrough_pair_v1* pair) {
  mf_cuda_passthrough_policy_v1 policy;
  mf_pt_path_snapshot current;
  char build[MF_PT_MAX_BUILD_TEXT + UINT32_C(1)];
  mf_cuda_passthrough_status_v1 status = MF_CUDA_PASSTHROUGH_SUCCESS;
  memset(&policy, 0, sizeof(policy));
  memset(&current, 0, sizeof(current));
  policy.struct_size = sizeof(policy);
  policy.abi_version = MF_CUDA_PASSTHROUGH_ABI_VERSION_V1;
  policy.trusted_uid = pair->trusted_uid;
  policy.enforce_trusted_ancestors = pair->enforce_trusted_ancestors;
  policy.proc_version_path = pair->proc_version_path;
  status = mf_pt_read_proc_version(&policy, &current, build);
  if (status == MF_CUDA_PASSTHROUGH_SUCCESS &&
      (mf_pt_path_snapshot_equal(&pair->proc_version, &current) == 0 ||
       strcmp(build, pair->proc_driver_build) != 0)) {
    status = MF_CUDA_PASSTHROUGH_STALE;
  }
  mf_pt_path_snapshot_reset(&current);
  return status == MF_CUDA_PASSTHROUGH_SUCCESS ? status : MF_CUDA_PASSTHROUGH_STALE;
}

mf_cuda_passthrough_status_v1
mf_cuda_passthrough_pair_validate_current_v1(const mf_cuda_passthrough_pair_v1* pair) {
  if (pair == NULL) {
    return MF_CUDA_PASSTHROUGH_INVALID_ARGUMENT;
  }
  if (pair->owner_pid != getpid() ||
      mf_pt_path_snapshot_is_current(pair->pid_namespace_path, &pair->pid_namespace) !=
          MF_CUDA_PASSTHROUGH_SUCCESS ||
      mf_pt_path_snapshot_is_current(pair->mount_namespace_path, &pair->mount_namespace) !=
          MF_CUDA_PASSTHROUGH_SUCCESS ||
      mf_pt_path_snapshot_is_current(pair->config_path, &pair->config) !=
          MF_CUDA_PASSTHROUGH_SUCCESS ||
      mf_pt_path_snapshot_is_current(pair->cuda.source_path, &pair->cuda.path) !=
          MF_CUDA_PASSTHROUGH_SUCCESS ||
      mf_pt_path_snapshot_is_current(pair->nvml.source_path, &pair->nvml.path) !=
          MF_CUDA_PASSTHROUGH_SUCCESS ||
      mf_pt_validate_proc_version(pair) != MF_CUDA_PASSTHROUGH_SUCCESS) {
    return MF_CUDA_PASSTHROUGH_STALE;
  }
  return MF_CUDA_PASSTHROUGH_SUCCESS;
}

const char* mf_cuda_passthrough_pair_cuda_path_v1(const mf_cuda_passthrough_pair_v1* pair) {
  return pair == NULL ? NULL : pair->cuda.path.resolved_path;
}

const char* mf_cuda_passthrough_pair_nvml_path_v1(const mf_cuda_passthrough_pair_v1* pair) {
  return pair == NULL ? NULL : pair->nvml.path.resolved_path;
}

const char* mf_cuda_passthrough_pair_driver_build_v1(const mf_cuda_passthrough_pair_v1* pair) {
  return pair == NULL ? NULL : pair->proc_driver_build;
}

int64_t mf_cuda_passthrough_pair_namespace_id_v1(const mf_cuda_passthrough_pair_v1* pair) {
  return pair == NULL ? INT64_C(-1) : (int64_t)pair->namespace_id;
}

const mf_cuda_passthrough_bootstrap_v1*
mf_cuda_passthrough_pair_bootstrap_v1(const mf_cuda_passthrough_pair_v1* pair) {
  return pair == NULL ? NULL : &pair->bootstrap;
}

mf_cuda_passthrough_status_v1
mf_cuda_passthrough_pair_lookup_v1(const mf_cuda_passthrough_pair_v1* pair,
                                   mf_cuda_vendor_library_v1 library, const char* symbol_name,
                                   const char* symbol_version, void** out_symbol) {
  void* handle = NULL;
  void* symbol = NULL;
  const char* error = NULL;
  if (out_symbol == NULL) {
    return MF_CUDA_PASSTHROUGH_INVALID_ARGUMENT;
  }
  *out_symbol = NULL;
  if (pair == NULL || symbol_name == NULL || symbol_name[0] == '\0' ||
      (symbol_version != NULL && symbol_version[0] == '\0') ||
      (library != MF_CUDA_VENDOR_LIBRARY_CUDA_V1 && library != MF_CUDA_VENDOR_LIBRARY_NVML_V1)) {
    return MF_CUDA_PASSTHROUGH_INVALID_ARGUMENT;
  }
  if (mf_cuda_passthrough_pair_validate_current_v1(pair) != MF_CUDA_PASSTHROUGH_SUCCESS) {
    return MF_CUDA_PASSTHROUGH_STALE;
  }
  handle = library == MF_CUDA_VENDOR_LIBRARY_CUDA_V1 ? pair->cuda_handle : pair->nvml_handle;
  (void)dlerror();
  symbol = symbol_version == NULL ? dlsym(handle, symbol_name)
                                  : dlvsym(handle, symbol_name, symbol_version);
  error = dlerror();
  if (error != NULL || symbol == NULL) {
    return MF_CUDA_PASSTHROUGH_SYMBOL_MISSING;
  }
  *out_symbol = symbol;
  return MF_CUDA_PASSTHROUGH_SUCCESS;
}

mf_cuda_passthrough_status_v1 mf_cuda_runtime_mode_parse_v1(const char* text,
                                                            mf_cuda_runtime_mode_v1* out_mode) {
  if (text == NULL || out_mode == NULL) {
    return MF_CUDA_PASSTHROUGH_INVALID_ARGUMENT;
  }
  *out_mode = UINT32_C(0);
  if (strcmp(text, "managed") == 0) {
    *out_mode = MF_CUDA_RUNTIME_MODE_MANAGED_V1;
  } else if (strcmp(text, "passthrough") == 0) {
    *out_mode = MF_CUDA_RUNTIME_MODE_PASSTHROUGH_V1;
  } else if (strcmp(text, "auto") == 0) {
    *out_mode = MF_CUDA_RUNTIME_MODE_AUTO_V1;
  } else {
    return MF_CUDA_PASSTHROUGH_INVALID_MODE;
  }
  return MF_CUDA_PASSTHROUGH_SUCCESS;
}

static int mf_pt_managed_callbacks_valid(const mf_cuda_managed_callbacks_v1* callbacks) {
  return callbacks != NULL && callbacks->struct_size == sizeof(*callbacks) &&
         callbacks->abi_version == MF_CUDA_PASSTHROUGH_ABI_VERSION_V1 &&
         callbacks->prepare != NULL && callbacks->commit != NULL && callbacks->rollback != NULL &&
         callbacks->is_pristine != NULL && callbacks->reserved[0] == UINT64_C(0) &&
         callbacks->reserved[1] == UINT64_C(0);
}

static mf_cuda_passthrough_status_v1
mf_pt_managed_cleanup(const mf_cuda_managed_callbacks_v1* callbacks, uint64_t ticket) {
  const mf_cuda_passthrough_status_v1 rollback = callbacks->rollback(callbacks->context, ticket);
  if (rollback != MF_CUDA_PASSTHROUGH_SUCCESS ||
      callbacks->is_pristine(callbacks->context) != INT32_C(1)) {
    return MF_CUDA_PASSTHROUGH_PARTIAL_STATE;
  }
  return MF_CUDA_PASSTHROUGH_SUCCESS;
}

mf_cuda_passthrough_status_v1 mf_cuda_runtime_select_with_policy_v1(
    mf_cuda_runtime_mode_v1 requested_mode, const mf_cuda_managed_callbacks_v1* managed,
    const mf_cuda_passthrough_policy_v1* policy, mf_cuda_selected_runtime_v1* out_selected,
    mf_cuda_passthrough_pair_v1** out_pair) {
  mf_cuda_passthrough_status_v1 status = MF_CUDA_PASSTHROUGH_SUCCESS;
  uint64_t ticket = UINT64_C(0);
  if (out_selected == NULL || out_pair == NULL) {
    return MF_CUDA_PASSTHROUGH_INVALID_ARGUMENT;
  }
  *out_selected = MF_CUDA_SELECTED_NONE_V1;
  *out_pair = NULL;
  if (mf_pt_policy_valid(policy) == 0 || (requested_mode != MF_CUDA_RUNTIME_MODE_MANAGED_V1 &&
                                          requested_mode != MF_CUDA_RUNTIME_MODE_PASSTHROUGH_V1 &&
                                          requested_mode != MF_CUDA_RUNTIME_MODE_AUTO_V1)) {
    return MF_CUDA_PASSTHROUGH_INVALID_ARGUMENT;
  }
  if (requested_mode == MF_CUDA_RUNTIME_MODE_PASSTHROUGH_V1) {
    status = mf_cuda_passthrough_pair_load_with_policy_v1(policy, out_pair);
    if (status == MF_CUDA_PASSTHROUGH_SUCCESS) {
      *out_selected = MF_CUDA_SELECTED_PASSTHROUGH_V1;
    }
    return status;
  }
  if (mf_pt_managed_callbacks_valid(managed) == 0) {
    return MF_CUDA_PASSTHROUGH_INVALID_ARGUMENT;
  }
  status = managed->prepare(managed->context, &ticket);
  if (status == MF_CUDA_PASSTHROUGH_SUCCESS && ticket == UINT64_C(0)) {
    status = MF_CUDA_PASSTHROUGH_INVALID_ARGUMENT;
  }
  if (status == MF_CUDA_PASSTHROUGH_SUCCESS) {
    status = managed->commit(managed->context, ticket);
  }
  if (status == MF_CUDA_PASSTHROUGH_SUCCESS) {
    *out_selected = MF_CUDA_SELECTED_MANAGED_V1;
    return MF_CUDA_PASSTHROUGH_SUCCESS;
  }
  {
    const mf_cuda_passthrough_status_v1 cleanup = mf_pt_managed_cleanup(managed, ticket);
    if (cleanup != MF_CUDA_PASSTHROUGH_SUCCESS) {
      return cleanup;
    }
  }
  if (requested_mode == MF_CUDA_RUNTIME_MODE_MANAGED_V1) {
    return status;
  }
  status = mf_cuda_passthrough_pair_load_with_policy_v1(policy, out_pair);
  if (status == MF_CUDA_PASSTHROUGH_SUCCESS) {
    *out_selected = MF_CUDA_SELECTED_PASSTHROUGH_V1;
  }
  return status;
}

mf_cuda_passthrough_status_v1 mf_cuda_runtime_select_v1(mf_cuda_runtime_mode_v1 requested_mode,
                                                        const mf_cuda_managed_callbacks_v1* managed,
                                                        mf_cuda_selected_runtime_v1* out_selected,
                                                        mf_cuda_passthrough_pair_v1** out_pair) {
  mf_cuda_passthrough_policy_v1 policy;
  char* self_path = NULL;
  mf_cuda_passthrough_status_v1 status = MF_CUDA_PASSTHROUGH_INVALID_ARGUMENT;
  if (out_selected == NULL || out_pair == NULL) {
    return MF_CUDA_PASSTHROUGH_INVALID_ARGUMENT;
  }
  *out_selected = MF_CUDA_SELECTED_NONE_V1;
  *out_pair = NULL;
  status = mf_pt_production_policy(&policy, &self_path);
  if (status == MF_CUDA_PASSTHROUGH_SUCCESS) {
    status = mf_cuda_runtime_select_with_policy_v1(requested_mode, managed, &policy, out_selected,
                                                   out_pair);
  }
  free(self_path);
  return status;
}
