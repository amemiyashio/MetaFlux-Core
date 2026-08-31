#include "metaflux/client/fastpath.h"
#include "metaflux/runtime/core.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include <linux/memfd.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace {

[[nodiscard]] int create_mapping(std::uint64_t size, void*& out_mapping) {
  const int fd =
      static_cast<int>(syscall(SYS_memfd_create, "metaflux-fastpath-registry-test", MFD_CLOEXEC));
  if (fd < 0 || ftruncate(fd, static_cast<off_t>(size)) != 0) {
    if (fd >= 0) {
      (void)close(fd);
    }
    return -1;
  }
  out_mapping =
      mmap(nullptr, static_cast<std::size_t>(size), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (out_mapping == MAP_FAILED) {
    (void)close(fd);
    return -1;
  }
  return fd;
}

[[nodiscard]] bool initialize(int fd, void* mapping, std::uint64_t size, bool zero,
                              std::uint64_t serial) {
  metaflux::runtime::RegistryView view;
  const mf_registry_view_id_v1 view_id{UINT64_C(0xf45), serial};
  mf_shared_status_v1 status = MF_SHARED_INVALID_ARGUMENT;
  if (zero) {
    status = metaflux::runtime::RegistryView::initialize(
        mapping, size, view_id, serial, std::span<const mf_virtual_device_identity_v1>{},
        std::span<const metaflux::runtime::FenceSnapshot>{}, view);
  } else {
    std::array<mf_virtual_device_identity_v1, 1> identities{};
    identities[0].identity_record_id = UINT64_C(5);
    identities[0].committed_generation = UINT64_C(1);
    std::array<metaflux::runtime::FenceSnapshot, 1> fences{{
        {UINT64_C(5), UINT64_C(1), UINT64_C(1), UINT64_C(4096), UINT64_C(0),
         MF_DEVICE_STATE_ONLINE},
    }};
    status = metaflux::runtime::RegistryView::initialize(mapping, size, view_id, serial, identities,
                                                         fences, view);
  }
  if (status != MF_SHARED_SUCCESS) {
    return false;
  }
  mf_client_registry_v1 client{};
  client.owned_fd = -1;
  const mf_shared_status_v1 attached = mf_client_registry_attach_v1(fd, view_id, &client);
  const bool valid = attached == MF_SHARED_SUCCESS &&
                     mf_client_registry_device_count_v1(&client) == (zero ? 0U : 1U) &&
                     mf_client_registry_process_view_revision_v1(&client) == serial;
  mf_client_registry_close_v1(&client);
  return valid;
}

[[nodiscard]] mf_shared_status_v1 attach_status(int fd, mf_registry_view_id_v1 view_id) {
  mf_client_registry_v1 client{};
  client.owned_fd = -1;
  const mf_shared_status_v1 status = mf_client_registry_attach_v1(fd, view_id, &client);
  mf_client_registry_close_v1(&client);
  return status;
}

} // namespace

int main() {
  for (std::uint32_t mode = 0; mode < 3U; ++mode) {
    const bool zero = mode == 2U;
    std::uint64_t size = 0;
    const mf_shared_status_v1 sized =
        mode == 0U
            ? metaflux::runtime::RegistryView::required_legacy_mapping_size(zero ? 0U : 1U, size)
            : metaflux::runtime::RegistryView::required_recovery_mapping_size(zero ? 0U : 1U, size);
    if (sized != MF_SHARED_SUCCESS) {
      return static_cast<int>(10U + mode);
    }
    void* mapping = nullptr;
    const int fd = create_mapping(size, mapping);
    if (fd < 0 || !initialize(fd, mapping, size, zero, mode + 1U)) {
      if (fd >= 0) {
        (void)munmap(mapping, static_cast<std::size_t>(size));
        (void)close(fd);
      }
      return static_cast<int>(20U + mode);
    }
    if (mode == 1U) {
      auto* header = static_cast<mf_shared_registry_header_v1*>(mapping);
      std::uint64_t legacy_size = 0;
      if (metaflux::runtime::RegistryView::required_legacy_mapping_size(1U, legacy_size) !=
          MF_SHARED_SUCCESS) {
        return 30;
      }
      auto* extension = reinterpret_cast<mf_shared_registry_extension_header_v1*>(
          static_cast<std::uint8_t*>(mapping) + legacy_size);
      auto* view_control = reinterpret_cast<mf_registry_view_control_v1*>(
          static_cast<std::uint8_t*>(mapping) + header->view_control_offset);
      const mf_shared_registry_header_v1 saved_header = *header;
      const mf_shared_registry_extension_header_v1 saved_extension = *extension;
      const mf_registry_view_control_v1 saved_view_control = *view_control;
      const mf_registry_view_id_v1 view_id{UINT64_C(0xf45), UINT64_C(2)};
      header->flags |= UINT32_C(0x80000000);
      if (attach_status(fd, view_id) != MF_SHARED_MALFORMED) {
        return 31;
      }
      *header = saved_header;
      extension->admission_attempt_capacity = UINT32_MAX;
      if (attach_status(fd, view_id) != MF_SHARED_MALFORMED) {
        return 32;
      }
      *extension = saved_extension;
      extension->device_updates_offset += 1U;
      if (attach_status(fd, view_id) != MF_SHARED_MALFORMED) {
        return 33;
      }
      *extension = saved_extension;
      extension->telemetry_publish_records_offset = UINT64_MAX - UINT64_C(63);
      if (attach_status(fd, view_id) != MF_SHARED_MALFORMED) {
        return 34;
      }
      *extension = saved_extension;
      header->device_count = UINT32_MAX;
      if (attach_status(fd, view_id) != MF_SHARED_MALFORMED) {
        return 35;
      }
      *header = saved_header;
      header->process_view_revision = UINT64_C(0);
      if (attach_status(fd, view_id) != MF_SHARED_MALFORMED) {
        return 36;
      }
      *header = saved_header;
      view_control->process_view_revision = UINT64_C(3);
      if (attach_status(fd, view_id) != MF_SHARED_MALFORMED) {
        return 37;
      }
      *view_control = saved_view_control;
    }
    (void)munmap(mapping, static_cast<std::size_t>(size));
    (void)close(fd);
  }
  return 0;
}
