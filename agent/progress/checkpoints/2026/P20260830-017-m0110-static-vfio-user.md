---
id: P20260830-017
status: Recorded
captured: 2026-08-30
milestone: M0110
workstream: W0113
branch: main
git_revision: 43b8d86b17f490644baaaa68ab5eab5ce13c447d
workspace: W0113 static vfio-user control-plane stage is implemented; guest PCI and steady-state data-plane gates remain active
---

# M0110 W0113 static vfio-user control stage

## Outcome

The vfio-user extension now has generated DMA map/unmap and GET_INFO reply
records, a C17 guest packet encoder/decoder, and a C++20 `SOCK_SEQPACKET`
server fixture. The server validates shared-file mappings against generation,
epoch, page alignment, range width, permissions, overlap, and fd size; it
reports the static BAR0/BAR2/BAR4 profile and keeps reset and migration
unsupported. The inherited 32-byte completion ABI is unchanged; GET_INFO uses
its generated 96-byte extension reply.

## Verification evidence

| Gate | Result |
|---|---|
| Transport schema | 5 definitions and 15 records; generated header and C/C++ schema fixtures passed |
| Component graph | 17 components and 18 dependency edges passed |
| Focused transport CTest | 7/7 passed: schema C/C++/validator, cdev client/worker, vfio-user guest/server |
| Full dev CTest | 72/72 passed |
| Server mapping fixture | GET_INFO, overlap rejection, unsupported reset, and `No_reply` unmap passed |

## Boundary

W0113 remains Active. Guest `metaflux_pci.ko`, pinned QEMU/libvfio-user
integration, BAR2 doorbell and BAR4 MSI-X steady state, CPU Add/Copy through
the guest path, DMA drain/tombstone faults, and package qualification remain
open. No reset or migration capability is advertised.

## Cleanup

- Removed: failed first-reply overflow route; no build artifacts were added to the repository.
- Retained: durable schema, guest/server sources, tests, plan updates, and compact session records.

## Handoff

Resume from W0113 with the generated GET_INFO contract and focused tests. Keep
the guest PCI/QEMU and steady-state data-plane work in a new checkpoint; do not
expand the inherited completion record.
