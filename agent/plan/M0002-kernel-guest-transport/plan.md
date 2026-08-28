---
id: M0002
legacy_id: "0002.1"
release: v0.2.1
status: Queued
depends_on: [M0001]
areas: [kernel.core, kernel.pci, transport.cdev, transport.vfio-user]
updated: 2026-08-28
---

# M0002: Kernel and Guest Transport

## Outcome

Prove local cdev and static QEMU/KVM vfio-user data paths with the M0001 CPU
backend before freezing the public transport envelope. Deliver
`metaflux_core.ko`, common guest `metaflux_pci.ko`,
`metaflux-vfio-userd`, DMA rings/data, ioeventfd/MSI-X, and unmodified Add/Copy on
both paths. CUDA/NVML remain the only application-facing APIs; this milestone is
not NVIDIA RM/UVM emulation.

Development beyond fixtures requires
[M0001](../M0001-core-foundation/plan.md) identity/view, 64-byte descriptor,
timeline, handle and `mf_atomic_*` rules, `mf_backend_api_v1` and CPU Add/Copy,
managed providers/activation, and the reproducible toolchain/ABI/benchmark
harness.

The sole-authority and leased-worker rules are defined once in
[the control/data-plane architecture](../../../docs/architecture/control-and-data-plane.md).
M0002 implements their local broker and guest transport consequences without
duplicating that architecture here.

## Locked Boundaries

- Validate Linux 6.12 LTS and 6.18 LTS. Kernel modules use target Kbuild GNU C;
  `metaflux-vfio-userd` uses C++20/libvfio-user; providers remain C17; boundaries
  use C ABI and fixed-width Linux UAPI. No Rust, assembly implementation, or C++
  ABI crosses components. CI uses each supported distribution compiler and
  `LLVM=1` where the target kernel configuration supports it.
- `contracts/protocol/transport/v1/schema/manifest.json` is the sole M0002
  base transport-envelope root. It carries a frozen base-definition allowlist
  and references each listed definition in
  `contracts/protocol/device/v1`, `contracts/protocol/transport/v1`,
  `contracts/shared/device/v1`, and `contracts/uapi/linux/v1` exactly once while
  each zone retains ownership of its bytes. Later extension-owned definitions are
  outside that base closure and are referenced only by their own extension
  manifests. Kernel, guest, server, C17, and C++20 layouts plus golden fixtures
  are generated projections; no layer-local struct or table is a second
  normative schema.
- memfd remains local-only fallback and may be selected before visible success
  only for cdev `ENOENT`, `ENODEV`, or explicit ABI incompatibility. Permission,
  malformed/integrity state, and policy rejection never silently fall back.
- Transport commits for the `provider initialization epoch`. CUDA and NVML in one
  process join one mode/transport/`registry_view_id`. Guests require cdev/vfio-user
  and fail closed.
- QEMU and `metaflux-vfio-userd` are one trusted high-speed boundary.
- Static guest cold-plug and terminal reset-to-`LOST` are included. M0002 leaves
  `VFIO_DEVICE_FLAGS_RESET` clear and advertises no migration capability; a
  guest/QEMU reset observation is terminal loss, not a supported reset reply.
  Coordinated replacement generations, reset advertisement, QMP hotplug, and
  bare-metal software PCI move to [M0003](../M0003-vpci-lifecycle/plan.md).
- Vulkan/SPIR-V/external Vulkan memory, cubin/SASS/`nvdisasm`, SR-IOV, ATS,
  PASID, PRI, P2P, AER, live migration, and transparent reconnect are excluded.
- Persistent MetaFlux UUID is cross-domain identity; BDF is stable only within
  its enumeration domain. PCI class is `0x120000`; CI VID/DID is
  `0x4D46:0x0001`; release identity must be registered or deployment supplied.
- CUDA/NVML/PCI/sysfs/cdev within one domain agree on UUID, BDF, name, quota,
  generation, and state.

## Scope

Included:

- Canonical `/dev/metafluxctl` and `/dev/metafluxN`, local worker broker,
  context/queue/mapping/registration/eventfd/wait/teardown UAPI.
- One static guest function per vfio-user socket; BAR0 control, BAR2 doorbell,
  BAR4 MSI-X table/PBA, two vectors, SPSC rings, and timelines.
- Coherent ring pages, explicitly registered buffers, CPU Add/Copy on local and
  guest paths, pinned-kernel/NixOS and generic DKMS packages, QEMU fixture, and
  guest image.

Excluded:

- `metaflux_vroot.ko`, bare-metal `lspci`, dynamic hotplug/re-add, public
  coordinated reset, lifecycle aliases, functional bare-metal BAR/IRQ/PM/PCIe/FLR.
- NVIDIA-named aliases/private RM/UVM ioctls, Vulkan, and cubin execution.

## Workstreams

| Workstream | Deliverable |
| --- | --- |
| [M0002-W01](work/W01-abi-benchmark-contract.md) | ABI 0.x fixtures, UAPI boundary, endian adaptation, and benchmark definition |
| [M0002-W02](work/W02-local-cdev.md) | Local cdev Add/Copy and exclusive worker broker |
| [M0002-W03](work/W03-static-vfio-user.md) | Static guest PCI, DMA, BAR, ioeventfd/MSI-X Add/Copy |
| [M0002-W04](work/W04-fault-abi-freeze.md) | Fault qualification and data-plane v1 freeze |
| [M0002-W05](work/W05-performance-release.md) | Performance, packaging, and release hardening |

## Milestone Acceptance

Correctness and ABI:

- Local cdev and guest Add/Copy agree with M0001.
- CUDA/NVML/PCI/sysfs/cdev agree per domain; UUID/generation agree across mapping,
  while host/guest BDF need only local stability.
- `VFIO_USER_DEVICE_GET_INFO` leaves `VFIO_DEVICE_FLAGS_RESET` clear and
  migration probing reports unsupported. A defensively received
  `VFIO_USER_DEVICE_RESET` never receives success; the pinned-pair matrix freezes
  its error reply or terminal close behavior before implementation. Guest/QEMU
  reset observation and disconnect publish `LOST` and require a fresh instance.
- vfio-user message IDs remain sender-owned values echoed in replies; they may be
  reused concurrently and receivers assume no uniqueness. `No_reply` suppresses
  only the reply, client commands execute in receive order, and opposite
  directions establish no global total order.
- Native/compat ABI, short structures, unknown extensions, null/overflow/stale
  generation, and concurrent teardown pass.
- Exactly one worker lease consumes a live generation; revoked/dead workers cannot
  attach replacement queues or publish completions.
- A successful DMA unmap proves no server/backend reference. Timeout yields
  `LOST` and connection failure, never false success.

Performance:

- Local warm dispatch p50 <= 1 microsecond, p99 <= 3 microseconds.
- Guest warm dispatch p50 <= 2 microseconds, p99 <= 8 microseconds.
- Uncontended active client path has zero syscall, heap allocation, and global
  lock. Same-stream contention and owner death are reported separately.
- Guest steady state has zero vfio-user socket message and no QEMU main-loop
  region write.
- Kernels >= 100 microseconds add <= 3% scheduling overhead against the same
  direct backend topology/policy.
- Pre-registered copies >= 16 MiB reach >= 90% of the locked direct CPU-copy
  harness over the same pages/NUMA/affinity with no whole-buffer extra copy.

Pin host CPU, guest vCPU, and NUMA; separate empty/busy and poll/block modes; and
archive raw distributions and the direct harness version.

Fault and release:

- Producer death, wrap/full, duplicates, stale mapping/generation, concurrent
  unmap, and non-completing backend are bounded.
- QEMU/server/daemon loss never leaves a half-online device.
- Vendor files/nodes remain untouched. Generic packages have no required
  `/nix/store` path and pass signing, udev, dependency, symbol, and uninstall
  checks.
- M0001 remains green with M0002 installed but idle.

## Decisions to Close

1. Exact base data-plane UAPI v1 and extension namespace.
2. Exact Linux, QEMU, and libvfio-user support matrix.
3. QEMU shared-memory command line and deployment ownership.
4. DMA width, pin quotas, ring-order range, drain deadlines, and interrupt
   moderation defaults.
5. Provider cdev selection and M0001 fallback diagnostics.
6. Registered release VID/DID process.
7. Module-signing and Secure Boot packaging workflow.

Items 1 and 4 close only after M0002-W04 evidence, not before implementation.

## Definition of Done

M0002 is complete when both CPU Add/Copy slices pass; the qualified transport
UAPI/device protocol/BAR envelope freezes as `v1` without changing the inherited
descriptor; DMA/ioeventfd/MSI-X/teardown have no steady-state control traffic;
performance/package gates and raw evidence pass; and M0001 stays green.

M0003 may then extend lifecycle/admin behavior without changing M0002 data-plane
v1. Future v0.3 semantic work that only needs stable backend/data-plane ABIs may
begin after this DoD; it does not wait for Vulkan or cubin research.

## References

- Linux external modules: <https://docs.kernel.org/kbuild/modules.html>
- Linux ioctl design: <https://docs.kernel.org/driver-api/ioctl.html>
- Linux internal driver ABI policy:
  <https://docs.kernel.org/process/stable-api-nonsense.html>
- QEMU vfio-user protocol:
  <https://www.qemu.org/docs/master/interop/vfio-user.html>
- QEMU vfio-user device:
  <https://www.qemu.org/docs/master/system/devices/vfio-user.html>
- QEMU multi-process shared memory:
  <https://www.qemu.org/docs/master/devel/multi-process.html>
- Linux user-page pinning:
  <https://docs.kernel.org/core-api/pin_user_pages.html>
