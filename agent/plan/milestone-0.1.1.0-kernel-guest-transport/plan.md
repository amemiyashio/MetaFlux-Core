---
id: milestone-0.1.1.0
delivery: 0.1.1.0
release: v0.1.1
status: Complete
depends_on: [milestone-0.1.0.0]
areas: [kernel.core, kernel.pci, transport.cdev, transport.vfio-user]
updated: 2026-09-06
---

# milestone-0.1.1.0: Kernel and Guest Transport

## Outcome

Prove local cdev and static QEMU/KVM vfio-user data paths with the milestone-0.1.0.0 CPU
backend before freezing the public transport envelope. Deliver
`metaflux_core.ko`, common guest `metaflux_pci.ko`,
`metaflux-vfio-userd`, DMA rings/data, ioeventfd/MSI-X, and unmodified Add/Copy on
both paths. CUDA/NVML remain the only application-facing APIs; this milestone is
not NVIDIA RM/UVM emulation.

Development beyond fixtures requires
[milestone-0.1.0.0](../milestone-0.1.0.0-core-foundation/plan.md) identity/view, 64-byte descriptor,
timeline, handle and `mf_atomic_*` rules, `mf_backend_api_v1` and CPU Add/Copy,
managed providers/activation, and the reproducible toolchain/ABI/benchmark
harness.

The sole-authority and leased-worker rules are defined once in
[the control/data-plane architecture](../../../docs/architecture/control-and-data-plane.md).
milestone-0.1.1.0 implements their local broker and guest transport consequences without
duplicating that architecture here.

## Locked Boundaries

- Validate Linux 6.12 LTS and 6.18 LTS. Kernel modules use target Kbuild GNU C;
  `metaflux-vfio-userd` uses C++20/libvfio-user; providers remain C17; boundaries
  use C ABI and fixed-width Linux UAPI. No Rust, assembly implementation, or C++
  ABI crosses components. CI uses each supported distribution compiler and
  `LLVM=1` where the target kernel configuration supports it.
- `contracts/protocol/transport/v1/schema/manifest.json` is the sole milestone-0.1.1.0
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
### Provider cdev selection and fallback diagnostics (decision-0030)

A local managed provider attempts cdev first and commits cdev only after both
the cdev data device and the matching Unix daemon session negotiate the
cdev-binding capability. The Unix session remains the control plane for
object-table registration and the cdev queue is the steady-state data plane;
the daemon binds that queue to the same session, view, and generation before
the provider can expose successful managed initialization. The provider never
switches transport after the initialization epoch commits. Before any object
or command becomes visible, `ENOENT`, `ENODEV`, and an explicitly reported ABI
incompatibility may select the existing memfd path; permission failures,
malformed or integrity failures, and policy rejection are terminal diagnostics.
A cdev registered-memory handle is opaque across the C ABI and is never cast
to a host pointer. The current CPU backend uses an explicitly bound cdev
payload mapping until a backend import-handle extension is separately
qualified.
- QEMU and `metaflux-vfio-userd` are one trusted high-speed boundary.
- Static guest cold-plug and terminal reset-to-`LOST` are included. milestone-0.1.1.0 leaves
  `VFIO_DEVICE_FLAGS_RESET` clear and advertises no migration capability; a
  guest/QEMU reset observation is terminal loss, not a supported reset reply.
  Coordinated replacement generations, reset advertisement, QMP hotplug, and
  bare-metal software PCI move to [milestone-0.1.2.0](../milestone-0.1.2.0-vpci-lifecycle/plan.md).
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
| [work-item-0.1.1.1](work/work-item-0.1.1.1-abi-benchmark-contract.md) | ABI 0.x fixtures, UAPI boundary, endian adaptation, and benchmark definition |
| [work-item-0.1.1.2](work/work-item-0.1.1.2-local-cdev.md) | Local cdev Add/Copy and exclusive worker broker |
| [work-item-0.1.1.3](work/work-item-0.1.1.3-static-vfio-user.md) | Static guest PCI, DMA, BAR, ioeventfd/MSI-X Add/Copy |
| [work-item-0.1.1.4](work/work-item-0.1.1.4-fault-abi-freeze.md) | Fault qualification and data-plane v1 freeze |
| [work-item-0.1.1.5](work/work-item-0.1.1.5-performance-release.md) | Performance, packaging, and release hardening |

## Milestone Acceptance

Correctness and ABI:

- Local cdev and guest Add/Copy agree with milestone-0.1.0.0.
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
- milestone-0.1.0.0 remains green with milestone-0.1.1.0 installed but idle.

## Decisions to Close

1. Exact base data-plane UAPI v1 and extension namespace.
2. Exact Linux, QEMU, and libvfio-user support matrix.
3. QEMU shared-memory command line and deployment ownership.
4. DMA width, pin quotas, ring-order range, drain deadlines, and interrupt
   moderation defaults.
5. Registered release VID/DID process.
6. Module-signing and Secure Boot packaging workflow.

Items 1 and 4 close only after work-item-0.1.1.4 evidence, not before implementation.

Convergence (2026-09-06): items 1-4 closed with the work-item-0.1.1.4 freeze and
live transport evidence; item 3 closed with the QEMU/libvfio-user bring-up row.
Items 5-6 (registered VID/DID, signing/Secure Boot) are release-package policy
surface owned by the milestone-1.0.0.0 stable manifest.

## Definition of Done

milestone-0.1.1.0 is complete when both CPU Add/Copy slices pass; the qualified transport
UAPI/device protocol/BAR envelope freezes as `v1` without changing the inherited
descriptor; DMA/ioeventfd/MSI-X/teardown have no steady-state control traffic;
performance/package gates and raw evidence pass; and milestone-0.1.0.0 stays green.

Convergence (2026-09-06): closed on the executing AMD host. Live cdev and
vfio-user bring-up rows passed with contract-bound samples; the named
non-reopening residuals ride the debug-kernel supply path
(`tools/run-debug-kernel-qualification.py` for KASAN/KCSAN/lockdep/kmemleak
soak and the KUnit guest boot), the release package matrix harness for real
dpkg/rpm rows, and optional huge-page/MSI-X storm soaks. None reopens the
frozen `v1` data-plane envelope; guest reboot and reference-host package rows
stay with the release matrix.

milestone-0.1.2.0 may then extend lifecycle/admin behavior without changing milestone-0.1.1.0 data-plane
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
