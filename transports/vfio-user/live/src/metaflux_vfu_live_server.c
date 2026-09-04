/*
 * Live bring-up fixture server for the static MetaFlux vfio-user function
 * (work-item-0.1.1.3). It presents the milestone-0.1.1.0 static PCI profile
 * through the host-qualified libvfio-user prerequisite recorded in
 * toolchains/vfio-user-1.json so the pinned QEMU can attach a static guest.
 *
 * This is qualification infrastructure, not the production
 * metaflux-vfio-userd. It exercises the live PCI identity, runtime BAR0
 * region access, the BAR2 doorbell ioeventfd, BAR4 MSI-X delivery, and
 * guest-RAM DMA registration end to end. No migration region exists and no
 * reset bit is advertised; a received reset is logged as terminal and is
 * never acknowledged as a supported reply.
 *
 * PCI identity and BAR/MSI-X sizes come only from
 * tools/generate-pci-guest-profile.py, which composes the root transport
 * vfio-user profile with the vroot CI Type-0 identity. No handwritten competing
 * layout is accepted in this fixture.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "metaflux/transport/generated.h"
#include "metaflux/transport/vfio_user_profile.h"

#include <libvfio-user.h>

#include <errno.h>
#include <inttypes.h>
#include <linux/pci_regs.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef METAFLUX_PROJECT_VERSION
#error "METAFLUX_PROJECT_VERSION must be supplied by the build"
#endif

/* CI Type-0 identity and BAR profile come only from the composed guest fixture. */
#include <metaflux/pci/generated_guest_profile.h>
#define MF_LIVE_VENDOR_ID ((uint64_t)MF_PCI_GUEST_VENDOR_ID)
#define MF_LIVE_DEVICE_ID ((uint64_t)MF_PCI_GUEST_DEVICE_ID)
#define MF_LIVE_CLASS_BASE ((uint8_t)((MF_PCI_GUEST_CLASS_CODE >> 16) & 0xffu))
#define MF_LIVE_CLASS_SUB ((uint8_t)((MF_PCI_GUEST_CLASS_CODE >> 8) & 0xffu))
#define MF_LIVE_CLASS_PROG_IF ((uint8_t)(MF_PCI_GUEST_CLASS_CODE & 0xffu))

/* BAR4 MSI-X layout: two-entry table at 0, PBA at 0x200 (QEMU owns both). */
#define MF_LIVE_MSIX_TABLE_OFFSET UINT64_C(0x0)
#define MF_LIVE_MSIX_PBA_OFFSET UINT64_C(0x200)

/* BAR0 trigger words used by the guest qualification script. */
#define MF_LIVE_TRIGGER_VECTOR0 UINT32_C(0x4d465558)
#define MF_LIVE_TRIGGER_VECTOR1 UINT32_C(0x4d465559)

/* Bounded guest DMA registration ledger for the fixture. */
#define MF_LIVE_MAX_DMA_REGIONS 512u

/*
 * Mirror of libvfio-user's pci_caps/msix.h capability layout for the pinned
 * prerequisite revision; the installed header set does not ship pci_caps/.
 */
struct mf_live_msix_cap {
  uint8_t id;
  uint8_t next;
  uint16_t control;
  uint32_t table;
  uint32_t pba;
} __attribute__((packed));

struct mf_live_state {
  vfu_ctx_t *ctx;
  int doorbell_fd;
  unsigned long long doorbell_writes;
  unsigned long long msix_triggers[MF_VFIO_USER_PROFILE_MSIX_VECTORS];
  unsigned long long dma_maps;
  unsigned long long dma_maps_mappable;
  unsigned long long dma_unmaps;
  int reset_type_seen;
  volatile sig_atomic_t stop;
};

static struct mf_live_state live_state = {
    .ctx = NULL,
    .doorbell_fd = -1,
    .doorbell_writes = 0U,
    .msix_triggers = {0U, 0U},
    .dma_maps = 0U,
    .dma_maps_mappable = 0U,
    .dma_unmaps = 0U,
    .reset_type_seen = -1,
    .stop = 0,
};

static void mf_live_log(const char *format, ...) {
  va_list arguments;
  va_start(arguments, format);
  (void)vprintf(format, arguments);
  va_end(arguments);
  (void)fflush(stdout);
}

static void mf_live_lib_log(vfu_ctx_t *ctx, int level, const char *message) {
  (void)ctx;
  (void)level;
  (void)fprintf(stderr, "LIVE:LIB %s\n", message);
  (void)fflush(stderr);
}

static void mf_live_request_stop(int signal_number) { (void)signal_number; live_state.stop = 1; }

static ssize_t mf_live_bar0_access(vfu_ctx_t *ctx, char *buf, size_t count, loff_t offset,
                                   bool is_write) {
  const uint32_t words[2] = {MF_TRANSPORT_MAGIC_V0,
                             ((uint32_t)MF_TRANSPORT_MAJOR_V0 << 16) | (uint32_t)MF_TRANSPORT_MINOR_V0};

  if (offset < 0 || (unsigned long long)offset + (unsigned long long)count >
                       (unsigned long long)MF_VFIO_USER_PROFILE_BAR0_SIZE ||
      ((unsigned long long)offset % UINT64_C(4)) != 0U || (count != 4U && count != 8U)) {
    mf_live_log("BAR0_REJECTED offset=%lld count=%zu write=%d\n", (long long)offset, count,
                is_write ? 1 : 0);
    errno = EINVAL;
    return -1;
  }

  if (!is_write) {
    memcpy(buf, ((const unsigned char *)words) + (size_t)offset, count);
    mf_live_log("BAR0_READ offset=%lld count=%zu value=%08" PRIx32 "\n", (long long)offset, count,
                words[(size_t)offset / 4U]);
    return (ssize_t)count;
  }

  if ((unsigned long long)offset == 0U) {
    uint32_t value = 0U;
    memcpy(&value, buf, sizeof(value));
    if (value == MF_LIVE_TRIGGER_VECTOR0 || value == MF_LIVE_TRIGGER_VECTOR1) {
      const uint32_t vector = (value == MF_LIVE_TRIGGER_VECTOR0) ? 0U : 1U;
      if (vfu_irq_trigger(ctx, vector) != 0) {
        mf_live_log("MSIX_TRIGGER_FAILED vector=%u errno=%d\n", (unsigned)vector, errno);
        errno = EIO;
        return -1;
      }
      live_state.msix_triggers[vector] += 1U;
      mf_live_log("MSIX_TRIGGERED vector=%u total=%llu\n", (unsigned)vector,
                  live_state.msix_triggers[vector]);
    } else {
      mf_live_log("BAR0_WRITE offset=0 value=%08" PRIx32 " ignored\n", value);
    }
  } else {
    mf_live_log("BAR0_WRITE offset=%lld count=%zu ignored\n", (long long)offset, count);
  }
  return (ssize_t)count;
}

static ssize_t mf_live_bar2_access(vfu_ctx_t *ctx, char *buf, size_t count, loff_t offset,
                                   bool is_write) {
  (void)ctx;
  (void)buf;
  if (offset < 0 || (unsigned long long)offset + (unsigned long long)count >
                       (unsigned long long)MF_VFIO_USER_PROFILE_BAR2_SIZE) {
    errno = EINVAL;
    return -1;
  }
  if (is_write) {
    /*
     * The pinned QEMU 10.2.4 client does not implement
     * VFIO_USER_DEVICE_SET_IOEVENTFD, so guest doorbell writes arrive as
     * region-write messages instead of the KVM ioeventfd fast path. Count
     * them here; the ioeventfd registration is still attempted for clients
     * that support it.
     */
    live_state.doorbell_writes += (unsigned long long)(count / 4U);
    mf_live_log("DOORBELL total=%llu path=region-write\n", live_state.doorbell_writes);
  }
  return (ssize_t)count;
}

static ssize_t mf_live_bar4_access(vfu_ctx_t *ctx, char *buf, size_t count, loff_t offset,
                                   bool is_write) {
  (void)ctx;
  if (offset < 0 || (unsigned long long)offset + (unsigned long long)count >
                       (unsigned long long)MF_VFIO_USER_PROFILE_BAR4_SIZE) {
    errno = EINVAL;
    return -1;
  }
  if (!is_write) {
    memset(buf, 0, count);
  }
  return (ssize_t)count;
}

static void mf_live_dma_register(vfu_ctx_t *ctx, vfu_dma_info_t *info) {
  (void)ctx;
  live_state.dma_maps += 1U;
  if (info->vaddr != NULL) {
    live_state.dma_maps_mappable += 1U;
  }
  mf_live_log("DMA_MAP iova=%016llx-%016llx len=%llu prot=%u mappable=%d total=%llu\n",
              (unsigned long long)(uintptr_t)info->iova.iov_base,
              (unsigned long long)((uintptr_t)info->iova.iov_base + (uintptr_t)info->iova.iov_len - 1U),
              (unsigned long long)info->iova.iov_len, (unsigned)info->prot,
              info->vaddr != NULL ? 1 : 0, live_state.dma_maps);
}

static void mf_live_dma_unregister(vfu_ctx_t *ctx, vfu_dma_info_t *info) {
  (void)ctx;
  live_state.dma_unmaps += 1U;
  mf_live_log("DMA_UNMAP iova=%016llx len=%llu total=%llu\n",
              (unsigned long long)(uintptr_t)info->iova.iov_base,
              (unsigned long long)info->iova.iov_len, live_state.dma_unmaps);
}

static int mf_live_reset(vfu_ctx_t *ctx, vfu_reset_type_t type) {
  (void)ctx;
  live_state.reset_type_seen = (int)type;
  mf_live_log("RESET_OBSERVED type=%d\n", (int)type);
  if (type == VFU_RESET_DEVICE) {
    /* milestone-0.1.1.0 advertises no reset: never acknowledge success. */
    errno = EOPNOTSUPP;
    return -1;
  }
  return 0;
}

static int mf_live_setup_device(vfu_ctx_t *ctx) {
  struct mf_live_msix_cap msix;
  int result;

  vfu_pci_init(ctx, VFU_PCI_TYPE_CONVENTIONAL, PCI_HEADER_TYPE_NORMAL, 0);
  vfu_pci_set_id(ctx, (uint16_t)MF_LIVE_VENDOR_ID, (uint16_t)MF_LIVE_DEVICE_ID,
                 (uint16_t)MF_LIVE_VENDOR_ID, (uint16_t)MF_LIVE_DEVICE_ID);
  vfu_pci_set_class(ctx, MF_LIVE_CLASS_BASE, MF_LIVE_CLASS_SUB, MF_LIVE_CLASS_PROG_IF);

  result = vfu_setup_region(ctx, VFU_PCI_DEV_BAR0_REGION_IDX,
                            (size_t)MF_VFIO_USER_PROFILE_BAR0_SIZE, mf_live_bar0_access,
                            VFU_REGION_FLAG_RW | VFU_REGION_FLAG_MEM, NULL, 0, -1, 0);
  if (result != 0) {
    return -1;
  }
  result = vfu_setup_region(ctx, VFU_PCI_DEV_BAR2_REGION_IDX,
                            (size_t)MF_VFIO_USER_PROFILE_BAR2_SIZE, mf_live_bar2_access,
                            VFU_REGION_FLAG_RW | VFU_REGION_FLAG_MEM, NULL, 0, -1, 0);
  if (result != 0) {
    return -1;
  }
  result = vfu_setup_region(ctx, VFU_PCI_DEV_BAR4_REGION_IDX,
                            (size_t)MF_VFIO_USER_PROFILE_BAR4_SIZE, mf_live_bar4_access,
                            VFU_REGION_FLAG_RW | VFU_REGION_FLAG_MEM, NULL, 0, -1, 0);
  if (result != 0) {
    return -1;
  }

  memset(&msix, 0, sizeof(msix));
  msix.id = (uint8_t)PCI_CAP_ID_MSIX;
  msix.next = 0U;
  msix.control = (uint16_t)(MF_VFIO_USER_PROFILE_MSIX_VECTORS - UINT32_C(1));
  /* Bits 2:0 carry the BIR; bits 31:3 carry the 8-byte-aligned offset. */
  msix.table = (uint32_t)4U | (uint32_t)MF_LIVE_MSIX_TABLE_OFFSET;
  msix.pba = (uint32_t)4U | (uint32_t)MF_LIVE_MSIX_PBA_OFFSET;
  if (vfu_pci_add_capability(ctx, 0, 0, &msix) < 0) {
    return -1;
  }

  if (vfu_setup_device_nr_irqs(ctx, VFU_DEV_MSIX_IRQ, MF_VFIO_USER_PROFILE_MSIX_VECTORS) != 0) {
    return -1;
  }
  if (vfu_setup_device_dma(ctx, MF_LIVE_MAX_DMA_REGIONS, mf_live_dma_register,
                           mf_live_dma_unregister) != 0) {
    return -1;
  }
  vfu_setup_device_reset_cb(ctx, mf_live_reset);
  return 0;
}

static int mf_live_wait_for_client(vfu_ctx_t *ctx) {
  for (unsigned attempt = 0U; attempt < 2400U && live_state.stop == 0; ++attempt) {
    struct pollfd descriptor;
    int result;
    descriptor.fd = vfu_get_poll_fd(ctx);
    descriptor.events = (short)(POLLIN | POLLOUT);
    descriptor.revents = 0;
    result = poll(&descriptor, 1U, 250);
    if (result < 0) {
      if (errno == EINTR) {
        continue;
      }
      return -1;
    }
    if (result == 0) {
      continue;
    }
    if (vfu_attach_ctx(ctx) == 0) {
      return 0;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      continue;
    }
    return -1;
  }
  errno = ETIMEDOUT;
  return -1;
}

static int mf_live_register_doorbell(vfu_ctx_t *ctx) {
  eventfd_t value = 0U;

  live_state.doorbell_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
  if (live_state.doorbell_fd < 0) {
    return -1;
  }
  if (vfu_create_ioeventfd(ctx, VFU_PCI_DEV_BAR2_REGION_IDX, live_state.doorbell_fd, 0,
                           MF_VFIO_USER_PROFILE_DOORBELL_WIDTH, 0, 0, -1, 0) != 0) {
    (void)close(live_state.doorbell_fd);
    live_state.doorbell_fd = -1;
    return -1;
  }
  while (eventfd_read(live_state.doorbell_fd, &value) == 0) {
    /* Drain any counter accumulated between creation and this read. */
  }
  return 0;
}

static void mf_live_drain_doorbell(void) {
  eventfd_t value = 0U;
  while (eventfd_read(live_state.doorbell_fd, &value) == 0) {
    live_state.doorbell_writes += (unsigned long long)value;
  }
  if (live_state.doorbell_writes != 0U) {
    mf_live_log("DOORBELL total=%llu\n", live_state.doorbell_writes);
  }
}

static int mf_live_serve(vfu_ctx_t *ctx) {
  while (live_state.stop == 0) {
    struct pollfd descriptors[2];
    int result;

    descriptors[0].fd = vfu_get_poll_fd(ctx);
    descriptors[0].events = POLLIN;
    descriptors[0].revents = 0;
    descriptors[1].fd = live_state.doorbell_fd;
    descriptors[1].events = POLLIN;
    descriptors[1].revents = 0;

    result = poll(descriptors, 2U, 250);
    if (result < 0) {
      if (errno == EINTR) {
        mf_live_drain_doorbell();
        continue;
      }
      mf_live_log("POLL_FAILED errno=%d\n", errno);
      return 1;
    }
    if ((descriptors[1].revents & POLLIN) != 0) {
      mf_live_drain_doorbell();
    }
    if ((descriptors[0].revents & POLLIN) != 0) {
      result = vfu_run_ctx(ctx);
      if (result < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          continue;
        }
        if (errno == ENOTCONN) {
          mf_live_log("DISCONNECT\n");
          return 0;
        }
        if (errno == EINTR) {
          continue;
        }
        mf_live_log("RUN_FAILED errno=%d\n", errno);
        return 1;
      }
    }
  }
  mf_live_log("STOPPED\n");
  return 0;
}

static int mf_live_run(const char *socket_path) {
  vfu_ctx_t *ctx;
  struct sigaction action;
  int result;

  memset(&action, 0, sizeof(action));
  action.sa_handler = mf_live_request_stop;
  sigemptyset(&action.sa_mask);
  if (sigaction(SIGINT, &action, NULL) != 0 || sigaction(SIGTERM, &action, NULL) != 0) {
    (void)fprintf(stderr, "metaflux-vfu-live-server: signal setup failed: %s\n", strerror(errno));
    return 1;
  }

  ctx = vfu_create_ctx(VFU_TRANS_SOCK, socket_path, LIBVFIO_USER_FLAG_ATTACH_NB, &live_state,
                       VFU_DEV_TYPE_PCI);
  if (ctx == NULL) {
    (void)fprintf(stderr, "metaflux-vfu-live-server: vfu_create_ctx failed: %s\n", strerror(errno));
    return 1;
  }
  live_state.ctx = ctx;
  if (vfu_setup_log(ctx, mf_live_lib_log, LOG_DEBUG) != 0) {
    (void)fprintf(stderr, "metaflux-vfu-live-server: vfu_setup_log failed: %s\n", strerror(errno));
    vfu_destroy_ctx(ctx);
    live_state.ctx = NULL;
    return 1;
  }
  if (mf_live_setup_device(ctx) != 0) {
    (void)fprintf(stderr, "metaflux-vfu-live-server: device setup failed: %s\n", strerror(errno));
    vfu_destroy_ctx(ctx);
    live_state.ctx = NULL;
    return 1;
  }
  if (vfu_realize_ctx(ctx) != 0) {
    (void)fprintf(stderr, "metaflux-vfu-live-server: vfu_realize_ctx failed: %s\n", strerror(errno));
    vfu_destroy_ctx(ctx);
    live_state.ctx = NULL;
    return 1;
  }
  mf_live_log("LIVE:LISTENING\n");

  if (mf_live_wait_for_client(ctx) != 0) {
    (void)fprintf(stderr, "metaflux-vfu-live-server: attach failed: %s\n", strerror(errno));
    vfu_destroy_ctx(ctx);
    live_state.ctx = NULL;
    return 1;
  }
  mf_live_log("ATTACHED\n");

  if (mf_live_register_doorbell(ctx) != 0) {
    (void)fprintf(stderr, "metaflux-vfu-live-server: doorbell ioeventfd failed: %s\n",
                  strerror(errno));
    vfu_destroy_ctx(ctx);
    live_state.ctx = NULL;
    return 1;
  }

  result = mf_live_serve(ctx);
  if (live_state.doorbell_fd >= 0) {
    mf_live_drain_doorbell();
    (void)close(live_state.doorbell_fd);
    live_state.doorbell_fd = -1;
  }
  mf_live_log("EXIT code=%d dma_maps=%llu mappable=%llu unmaps=%llu doorbells=%llu msix=%llu/%llu "
              "reset_type=%d\n",
              result, live_state.dma_maps, live_state.dma_maps_mappable, live_state.dma_unmaps,
              live_state.doorbell_writes, live_state.msix_triggers[0], live_state.msix_triggers[1],
              live_state.reset_type_seen);
  vfu_destroy_ctx(ctx);
  live_state.ctx = NULL;
  return result;
}

int main(int argc, char **argv) {
  if (argc == 2 && strcmp(argv[1], "--version") == 0) {
    (void)printf("metaflux-vfu-live-server %s\n", METAFLUX_PROJECT_VERSION);
    return 0;
  }
  if (argc != 2 || argv[1][0] == '\0') {
    (void)fprintf(stderr, "usage: metaflux-vfu-live-server [--version] SOCKET_PATH\n");
    return 64;
  }
  return mf_live_run(argv[1]);
}
