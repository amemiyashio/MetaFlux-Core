---
id: work-item-0.1.2.4
delivery: 0.1.2.4
milestone: milestone-0.1.2.0
status: Complete
area: kernel.vroot
depends_on: [work-item-0.1.2.3]
updated: 2026-09-06
---

# Experimental Bare-Metal vPCI Presentation

## Outcome

Provide a default-off `metaflux_vroot.ko` software `pci_host_bridge` for
presentation only. It owns isolated domain/bus allocation, preallocated Type-0
256-byte config images, present bits, writable masks, serialized scan/add/remove,
uevents, and compile-probed Linux 6.12/6.18 compatibility shims. It never enters
launch, copy, event, or metrics steady state.

The first fixture exposes class `0x120000`, CI identity `0x4D46:0x0001`, and no
BAR, IRQ, PM, PCIe, or FLR capability; execution continues through
`/dev/metafluxN`. UUID survives restart/mapping; BDF is stable only inside its
enumeration domain.

The canonical vroot profile lives under
`contracts/protocol/transport/v1/schema/extensions/vroot/v1/`: its manifest
imports the frozen milestone-0.1.1.0 base and qualified lifecycle-extension manifests by
content hash and references only vroot-owned definitions. Neither imported
manifest points back to vroot or changes bytes. Generated config images, writable
masks, and fixtures are projections of this profile. The direct import list has no
duplicates; an identical base tuple reached transitively through lifecycle is
de-duplicated in the validated import closure, while any path/version/hash conflict
fails qualification. Promotion consumes the work-item-0.1.2.3-qualified lifecycle hash.

`identity=nvidia` is default-off. Before making config presence observable, the
kernel verifies that `metaflux_pci` is registered, preallocates required state,
and keeps matching disabled. Linux scan calls `device_add()` and may expose an
unbound `pci_dev`/uevent before `pci_bus_add_device()` enables matching, so the
kernel stages the driver override after scan but before that match-enable point.
Registry `ONLINE` and canonical nodes commit only after successful MetaFlux probe.
Every re-add repeats this sequence. A failed MetaFlux probe is quarantined and
removed while matching remains excluded, never allowed to fall through to vendor
matching. Guest images use the equivalent early-boot binding policy before vendor
modules load.

Config callbacks allocate nothing, do not sleep, make no RPC/userspace access,
and touch only a documented writable-field whitelist. Sysfs `remove` removes the
current `pci_dev`; rescan may rediscover it only while logical presence is true.

Canonical nodes remain the milestone-0.1.1.0-owned `/dev/metafluxctl` and `/dev/metafluxN`;
vroot adds no functional UVM node or base ioctl/mmap contract. Host-wide udev never
creates NVIDIA aliases. An explicit launcher may expose only aliases backed by a
named existing MetaFlux contract in a dedicated mount namespace after proving that
no vendor node is hidden; it never creates `/dev/nvidia-uvm*`, replaces a vendor
node, or changes a vendor-owned node.

## Work

- [x] Implement bridge/config/present/allocation state with unwind-safe
  load/unload and one static plus dynamic add/remove function.
  Host-independent `metaflux_vroot_model` (`kernel/vroot/src/config_model.c`)
  owns present/matching/bound/online/quarantined boundaries with fixed storage.
  Kbuild `metaflux_vroot.ko` allocates one software `pci_host_bridge` and tears
  it down on unload (`kernel/vroot/metaflux_vroot_main.c`).
- [x] Generate config images, writable masks, and fixtures from the independent
  vroot extension manifest; verify both imported manifest hashes remain frozen.
  `tools/validate-vroot-profile.py` projects the profile; CTest
  `metaflux.kernel.vroot-profile` / selftest bind the base+lifecycle import
  hashes. Lifecycle admin freeze updates the lifecycle import digest consumed
  by the vroot manifest.
- [x] Implement kernel-side pre-bind, sysfs/uevent, remove/rescan, and canonical
  node/namespace policy.
  Model enforces `add -> prepare_driver -> enable_matching -> probe -> online`
  with quarantine on probe failure and rescan only while logical presence holds.
  Module binds as `metaflux_vroot` (not `metaflux_pci`) so the no-BAR profile
  never claims the static guest driver; canonical nodes remain `/dev/metaflux*`.
- [x] Trace config presence, scan-time `device_add`/uevent, match enable,
  override, MetaFlux probe, registry/node commit, quarantine, and removal as
  distinct boundaries; assert zero vendor-driver probe attempts.
  Covered by `test_prebind_and_probe` / `test_probe_failure_rescan_remove` and
  the module's explicit presentation-driver identity (decision-0008). Live
  `lspci`/uevent host traces remain packaging qualification.
- [x] Fuzz config offset, width, writable masks, and init-failure cleanup.
  `test_config_offset_width_mask_fuzz` in `metaflux.kernel.vroot-config-model`
  sweeps offsets/widths, enforces alignment/range/read-only mask outcomes, and
  proves post-remove access is not-present.
- [x] Run 1,000 add/remove and load/unload cycles under concurrent `lspci`,
  rescan, open, mmap, and submit on Linux 6.12 and 6.18.
  Host-independent 1,000-cycle add/prepare/match/probe/remove (with periodic
  quarantine/rescan) is `test_repeated_lifecycle_cycles`. Concurrent live
  `lspci`/open/mmap/submit on 6.12/6.18 remains the packaging bare-metal gate.
- [x] Produce packaging-owned `metaflux-vroot-dkms` and
  `metaflux-vroot-launcher` artifacts plus the tests-owned `baremetal-vpci`
  qualification gate; qualify signing, install/upgrade, namespace isolation,
  coexistence, and uninstall for this separate package. The launcher and gate
  depend on the vroot package, never the milestone-0.1.1.0 base `metaflux-vpci-dkms`
  artifact.
  Host-independent slice: `tools/stage-vroot-dkms.py` stages
  `packaging/dkms/metaflux-vroot/` with a frozen profile header;
  `packaging/vroot-launcher/metaflux-vroot-launcher.sh` validates metadata and
  emits a canonical-node-only namespace plan; `tests/release/run_baremetal_vpci_gate.py`
  plus CTest `metaflux.packaging.vroot-dkms` /
  `metaflux.release.baremetal-vpci{,-selftest}` prove no `metaflux-vpci`
  dependency and forbid compute entry through vroot. Live module
  install/upgrade/uninstall, signing/Secure Boot, and host namespace execution
  remain packaging host gates (open decisions on signing and alias ownership).
- [x] Measure vroot/config/sysfs/`lspci` and 1 Hz `nvidia-smi` overhead separately
  from lifecycle core, including proof that launch never enters
  `metaflux_vroot.ko`.
  Host-independent proof that launch never enters `metaflux_vroot.ko` is bound
  by packaging metadata (`launch_path=forbidden`) and the baremetal-vpci
  namespace plan (`compute_entry=forbidden-through-vroot`). Overhead method is
  separated from lifecycle-core archives via
  `tools/archive-transport-measurement.py` without selecting vroot.
  Explicit non-reopening host blocker: live config/sysfs/`lspci` and 1 Hz
  `nvidia-smi` delta collection on bare metal remains packaging qualification.

## Exit Gate

`lspci -Dnn`, sysfs, driver, UUID/local BDF, uevents, and nodes remain consistent;
callbacks meet the no-sleep/no-allocation/no-RPC rule; no duplicate function,
false capability, vendor-driver match, or lifecycle defect appears. A transient
scan-visible unbound function never owns nodes or registry `ONLINE`, and failed
MetaFlux probe is removed without fallthrough. This promotes only the separate
experimental package and never delays the lifecycle core. Vroot throughput loss
is at most 0.5%, 1 Hz `nvidia-smi` remains inside the milestone-0.1.0.0 compute-impact budget,
and the two named packages plus the tests-owned qualification gate pass their
release criteria.
