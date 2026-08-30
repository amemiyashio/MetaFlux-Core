---
id: P20260831-040
status: Recorded
captured: 2026-08-31
milestone: M0110
workstream: W0113
branch: main
git_revision: cb118f141af3835c33305662bf8eca03fd846ef2
workspace: static guest PCI resource binder now compiles and reserves the canonical resources; guest data plane and QEMU integration remain open
---

# M0110 W0113 Static Guest PCI Binder

## Outcome

The first kernel PCI stage for W0113 is recorded at content revision `cb118f1`.
`metaflux_pci.ko` validates the canonical CI VID/DID `0x4D46:0x0001`, class
`0x120000`, and exact BAR0/BAR2/BAR4 memory sizes before enabling the device. It
maps BAR0 and BAR2, reserves exactly two MSI-X vectors, leaves BAR4 to the PCI
MSI-X capability, and clears bus mastering before reverse-order remove teardown.

This is a static resource binder only. It does not claim a BAR2 doorbell
handler, MSI-X handler, guest SPSC ring, DMA map/unmap lifetime, QEMU or pinned
libvfio-user integration, Add/Copy execution, or reset/disconnect qualification.

## Verification evidence

| Gate | Result |
|---|---|
| Target Kbuild | Passed: Linux 6.18.42 GCC built `metaflux_pci.ko`; modpost and BTF completed |
| Module identity | Passed: alias `pci:v00004D46d00000001sv*sd*bc*sc*i*`; vermagic `6.18.42-1-cachyos-lts` |
| Full development CTest | Passed: 79/79 through `nix develop . --command ctest --preset dev` |
| Transport schema | Passed: 5 definitions / 15 records |
| Component graph | Passed: 19 components / 23 dependency edges |
| Diff check | Passed: `git diff --check` |
| Commit identity | Passed: Agent Harness (codex) as Author and Committer for `cb118f1` |

## Boundary

The W0113 plan's full static guest item remains unchecked. The next increments
must add the pinned QEMU/libvfio-user boundary, guest ring and doorbell path,
DMA reference draining/tombstones, and end-to-end Add/Copy evidence before the
work item can close.

## Cleanup

- Removed: none; target-kernel Kbuild output is external and ignored.
- Retained: PCI source, Makefile, documentation, plan update, and this checkpoint.

## roast

### light roasts

- Static guest PCI resource binder -> `kernel/pci/metaflux_pci_main.c`
  (content `cb118f1`; target Kbuild and module metadata)
- PCI ownership and bounded-stage documentation -> `kernel/pci/README.md` and
  `kernel/README.md` (content `cb118f1`; full CTest 79/79)

### medium roasts

- none.

### dark roasts

- none.

## session-only

- GCC minor-version mismatch warning from the target kernel build - reason:
  host-local evidence with no supported-toolchain policy change.

## Handoff

Resume S0113/W0113 by reading the W0113 plan, the transport manifest,
`kernel/pci/README.md`, and the PCI/vfio-user skills. Keep reset and migration
unadvertised in M0110 and treat this module as resource binding, not as a
complete guest transport.
