---
id: P20260831-068
status: Recorded
captured: 2026-08-31
milestone: M0110
workstream: W0111
branch: main
git_revision: fa515bb
workspace: host-independent vfio-user transport ABI negotiation
---

# M0110 W0111 Transport Negotiation Checkpoint

## Outcome

The vfio-user fixture now executes the transport ABI 0.x negotiation path. The
guest codec validates a request containing only candidate version and feature
bits. The server rejects unsupported versions or required features with the
common completion status, and successful negotiation returns the canonical
negotiation record with selected features, registry identity, generation, queue,
DMA, and in-flight limits. Negotiation control messages accept no file
descriptor.

## Verification evidence

| Gate | Result |
|---|---|
| Transport schema validator | Passed: 5 definitions / 15 records |
| Focused transport/schema/component/lifecycle tests | Passed: 8/8 |
| Full development CTest | Passed: 84/84 |
| CMake build and diff checks | Passed |
| Content identity | `fa515bb`, Agent Harness (codex) as Author and Committer |

## Boundary

This proves the host-independent negotiation codec and server selection logic.
It does not prove pinned QEMU/libvfio-user activation, live PCI BAR/MSI-X,
kernel cdev negotiation, guest Add/Copy, physical DMA lifetime, or NVIDIA
qualification.

## Cleanup

- Removed: no session-owned disposable artifacts.
- Retained: canonical schema, codec/server implementation, focused regression,
  and this checkpoint.

## roast

### light roasts

- Negotiation codec and server capability selection -> `fa515bb`; focused/full
  CTest evidence.

### medium roasts

- W0111 host-independent transport negotiation boundary ->
  `agent/plan/M0110-kernel-guest-transport/work/W0111-abi-benchmark-contract.md`;
  live activation remains open.

### dark roasts

- none.

## session-only

- Socketpair and control fixture - reason: validates protocol shape and feature
  intersection without representing hardware qualification.

## Handoff

Resume W0111 from `fa515bb`. Bind the negotiated identity and limits to the
local cdev and pinned QEMU/libvfio-user activation harness, then add measured
negotiation and data-plane timing evidence.
