---
id: M0003
legacy_id: "0002.2"
release: v0.2.2
status: Queued
depends_on: [M0002]
areas: [lifecycle, kernel.vroot, presentation.vpci]
kernel_validation: [Linux 6.12 LTS, Linux 6.18 LTS]
updated: 2026-08-28
---

# M0003: Lifecycle and vPCI Presentation

## Outcome

Add one authoritative reset/hotplug lifecycle to the static M0002 transports,
then qualify an independent, default-off bare-metal PCI presentation package.
Admin reset, remove/re-add, QMP hotplug, disconnect/restart, generation/epoch
invalidation, canonical nodes, and 1,000-cycle qualification are included. The
M0002 descriptor, base ioctl/mmap UAPI, BAR layout, and steady-state ring remain
unchanged.

[M0002](../M0002-kernel-guest-transport/plan.md) completion is a prerequisite.
The sole-authority/data-plane-worker model is defined in
[the architecture record](../../../docs/architecture/control-and-data-plane.md);
M0003 adds only lifecycle transactions and milestone evidence around it.

## Locked Boundaries

- `metafluxd` alone reserves generation candidates, publishes committed
  generations, advances epoch, and owns the persistent registry transaction,
  policy, and generation-bound worker leases.
  Transport/kernel owners may transition one way to `LOST` and retain tombstones,
  but may not publish replacement `ONLINE` state.
- An accepted nonduplicate reset durably consumes exactly one generation
  candidate even if later staging or pre-transaction work fails. Epoch is not a
  candidate: it advances exactly once when the atomic replacement transaction
  retires the old generation and installs the candidate, and every
  pre-transaction failure leaves it unchanged.
- The lifecycle extension is authored once under
  `contracts/protocol/transport/v1/schema/extensions/lifecycle/v1/`. Its own
  manifest imports the frozen M0002 root manifest by content hash; the M0002 root
  never imports the extension or changes bytes. M0003-W01 must run the repository
  model checker with versioned bounds and produce its bounded JSON model-check
  evidence before any transport or PCI lifecycle adapter is implemented.
- A non-reusable `daemon_incarnation_id` identifies the coordinator process and
  remains separate from persistent UUID, generation, and epoch. Lifecycle
  requests are deadline-bound and replay-idempotent.
- M0001 memfd, M0002 local cdev, and M0002 guest vfio-user share one generation
  coordinator and enumeration-freeze behavior.
- PCI class remains `0x120000`; CI VID/DID remains `0x4D46:0x0001` and release
  identity is registered or deployment supplied. `identity=nvidia` is
  default-off, is a presentation disguise rather than a vendor-private ABI
  claim (D0008). Before scan, the kernel verifies the MetaFlux driver and keeps
  matching disabled. Linux scan may expose an unbound `pci_dev` through
  `device_add()`; the override is staged before `pci_bus_add_device()` enables
  matching, and registry `ONLINE` follows only successful MetaFlux probe. A
  post-enumeration userspace `driver_override` and all vendor-driver matching are
  unsupported.
- UUID is stable across restart and guest mapping. Host and guest BDFs are stable
  only inside their respective enumeration domains.
- Authoritative nodes remain the M0002-owned `/dev/metafluxctl` and
  `/dev/metafluxN`. M0003 adds no functional UVM node or base ioctl/mmap surface.
  Contracted NVIDIA-named aliases are permitted only in an explicit, isolated
  mount namespace and never replace a vendor/package-owned node;
  `/dev/nvidia-uvm*` aliases are excluded until a separately owned UVM contract
  exists.
- Bare-metal vroot is presentation-only, default-off, and has a separate package
  promotion gate. Its failure does not delay the lifecycle core or M0004.

## Scope

Included:

- Admin add, remove, drain, and reset on all existing transports.
- QMP add/remove, `VFIO_USER_DEVICE_RESET`, QEMU/server/daemon disconnect and
  restart, guest unload/reload, tombstones, and provider enumeration freeze.
- Experimental `metaflux_vroot.ko` software PCI root, config/sysfs/uevent,
  canonical udev nodes, namespace launcher, DKMS/NixOS packaging, and fault tests.

Excluded:

- Vulkan and mixed-backend residency; live migration and preservation of guest
  contexts; cubin/SASS input.
- Standard PCI FLR, AER, PM, PCIe performance capabilities, functional
  bare-metal BAR/interrupts, SR-IOV, ATS/PASID/PRI, and P2P.
- Host-wide NVIDIA aliases and NVIDIA RM/UVM private ioctl behavior.

## Workstreams

| Workstream | Deliverable |
| --- | --- |
| [M0003-W01](work/W01-lifecycle-model.md) | Idempotent lifecycle model, publication transactions, and ABI 0.x fixtures |
| [M0003-W02](work/W02-existing-transports.md) | Reset/hotplug on memfd, cdev, and guest vfio-user |
| [M0003-W03](work/W03-core-qualification.md) | 1,000-cycle core qualification and lifecycle v1 freeze |
| [M0003-W04](work/W04-experimental-vroot.md) | Default-off software root, presentation, pre-bind, and aliases |
| [M0003-W05](work/W05-performance-release.md) | Lifecycle-core performance, packaging, and release evidence independent of vroot |

## Acceptance Flows

Bare-metal experimental path:

```text
load metaflux_core, metaflux_pci, metaflux_vroot
  -> coordinator adds one logical function
  -> lspci/sysfs enumerate class, VID/DID, and local BDF
  -> metaflux_pci binds and canonical nodes appear
  -> unmodified CPU-backed CUDA Add/Copy succeeds
  -> reset invalidates old objects and commits a new generation
  -> remove publishes DEVICE_LOST and reaches ABSENT
```

Guest path:

```text
start static M0002 guest path
  -> normalize reset or QMP remove
  -> reject new work and publish DEVICE_LOST
  -> remove transport while retaining tombstones
  -> QMP add stages a complete replacement
  -> coordinator commits a new generation
  -> newly initialized guest provider executes CPU Add/Copy
```

## Milestone Acceptance

Lifecycle correctness:

- Every accepted nonduplicate reset consumes one candidate generation. After all
  replacement owners stage, one atomic transaction retires the old generation,
  increments epoch exactly once, and installs exactly that candidate `ONLINE`.
  A later fault marks the committed candidate `LOST`; duplicate requests consume
  none and failed staging candidates are never reused.
- Transport loss alone preserves the current generation and epoch as a `LOST`
  tombstone. Accepted recovery consumes one candidate, retires that old lost
  generation with exactly one epoch increment, and installs the candidate in the
  same atomic transaction. A pre-transaction failure leaves the old generation
  current, epoch unchanged, and state `LOST`.
- Normal remove retires the current generation with exactly one epoch increment
  before reaching `ABSENT`; a later add reserves a new generation candidate.
- Re-add receives a new generation while daemon incarnation remains separate.
  Old fd, VMA, queue, mapping, event, memory, executable, and handle objects
  return `DEVICE_LOST` and never refer to replacement backing.
- All event sources enter the same state machine. Memfd, cdev, and vfio-user
  observe the same commit/`DEVICE_LOST` boundary. Normal removal reaches
  `ABSENT`; reset and recover finish in complete `ONLINE` or `LOST`.
- CUDA and NVML in one process share one `registry_view_id`. Removal/loss updates
  frozen entries immediately. Re-add never creates a CUDA ordinal in an
  initialized process and appears in NVML only after a later zero-to-one init
  epoch or in a new process. Default unfiltered membership/order agrees only when
  both providers captured the same process-view revision. If NVML reinitializes
  while CUDA remains initialized, their count/order may diverge; common live
  incarnations still match by `(UUID, generation)`. `CUDA_VISIBLE_DEVICES` may
  filter/reorder CUDA only.

Identity and presentation:

- Every common live incarnation agrees on `(UUID, generation)`, name, capabilities,
  and generation-tagged state. Persistent UUID/logical ID correlates replacements,
  but UUID or BDF alone never equates old and new generations. Host and guest BDFs
  are independently stable and need not match.
- For stock R535/R550/R570/R580/R610 `nvidia-smi`, `-L`, default summary, and
  core queries agree with CUDA/cdev and guest PCI/sysfs for common live
  incarnations and for providers on the same revision. Loss reports the old
  generation consistently. After re-add, parity is re-established in a new
  process or after all participating providers capture the replacement revision;
  an old initialized CUDA view may remain lost while a later NVML epoch lists the
  replacement. Bare-metal PCI/sysfs joins only for vroot promotion.
- A synthetic NVIDIA identity is never eligible for a vendor driver. Failed
  pre-scan validation publishes no config-present function. After config presence,
  Linux `device_add()` may expose a transient unbound `pci_dev`, but matching
  remains disabled until the kernel-staged MetaFlux override is ready. Failed
  MetaFlux probe is quarantined and removed, never falls through, creates no
  canonical node, and never commits registry `ONLINE`.
- Unsupported PCI/NVIDIA capabilities are absent, not simulated. Promoted vroot
  keeps `lspci`, sysfs, driver binding, uevents, and nodes stable for 1,000 cycles.

Performance and release:

- M0002 warm-dispatch latency remains within its original local and guest bounds.
- Lifecycle-core same-backend steady-state throughput loss is at most 0.5%.
- vroot enablement is separately at most 0.5% before package promotion.
- Config/sysfs queries and 1 Hz `nvidia-smi` remain inside the M0001 compute
  impact budget; steady-state launch never enters `metaflux_vroot.ko`.
- Real vendor nodes/libraries remain untouched. Generic artifacts require no
  `/nix/store` runtime path.

## Decisions to Close

1. Exact lifecycle deadline and old-work isolation policy.
2. Persistence format for the generation-candidate high-water mark, committed
   retirement epoch, and `daemon_incarnation_id`.
3. Bare-metal domain/bus/devfn allocation and maximum logical functions.
4. Release VID/DID and optional custom identity workflow, including the
   registration and legal review that a synthetic NVIDIA presentation identity
   requires before any release promotion.
5. Exact supported kernel/distribution matrix.
6. Namespace launcher ownership and alias allowlist.
7. Module-signing and Secure Boot workflow.

Items 1-2 freeze only after M0003-W03 core qualification. Item 3 freezes only for
the experimental vroot package after M0003-W04 promotion evidence. Items 6-7 gate
M0003-W04 package promotion; item 7 also gates M0003-W05 core release packaging.

## Definition of Done

The lifecycle core is complete when M0002 remains green with no data-plane ABI
change; every source uses the single coordinator; memfd/cdev/guest 1,000-cycle
suites pass; old objects deterministically return `DEVICE_LOST`; identity,
performance, packaging, and fault gates pass; stock `nvidia-smi` stays consistent;
the canonical bounded-model command and JSON evidence pass; and
`mf_admin_lifecycle_v1` freezes only after qualification. M0004 release
integration depends on this core DoD, not vroot promotion.

The experimental vroot package is promoted only when Linux 6.12/6.18
`lspci`/sysfs/config suites pass; add/remove and load/unload each survive 1,000
concurrent-use cycles; and no duplicate device, leak, warning, hung task, false
BAR/IRQ claim, or hot-path regression remains.

## References

- Linux PCI driver API: <https://docs.kernel.org/driver-api/pci/pci.html>
- Linux PCI endpoint distinction:
  <https://docs.kernel.org/PCI/endpoint/pci-endpoint.html>
- Linux external modules: <https://docs.kernel.org/kbuild/modules.html>
- QEMU vfio-user protocol:
  <https://www.qemu.org/docs/master/interop/vfio-user.html>
- QEMU security model:
  <https://www.qemu.org/docs/master/system/security.html>
