---
id: milestone-0.2.0.0
delivery: 0.2.0.0
release: v0.2.0
status: Queued
budgets: provisional
depends_on: [milestone-0.1.0.0]
areas: [compat.cuda, compiler.cpu, backend.cpu, backend.vulkan, compatibility]
updated: 2026-09-06
---

# milestone-0.2.0.0: PyTorch CUDA Compatibility

## Outcome

Make unmodified PyTorch CUDA clients fully usable through MetaFlux: torch
enumerates the virtual device, loads its kernels, and executes eager CUDA
operations end-to-end with bit-exact results through the managed daemon, on
the CPU backend first and through daemon-routed Vulkan execution as the
second target.

The milestone occupies the `v0.2.0` delivery slot; the native NixOS
VM/package support expansion moves to the reserved `v0.3.0` slot. It promotes the
[PyTorch compatibility roadmap](../pytorch-compatibility-roadmap.md) probes
into a formal product line. Probe-only evidence stays diagnostic until a work
item here records it as qualification evidence.

## Starting Evidence (2026-09-06)

The baseline probe (PyTorch 2.11.0+cu126, CPython 3.13.15, warm-jit daemon)
passed the import stage and failed driver enumeration: torch reports zero
devices with driver error 36 from its cudart initialization sequence, while
direct driver-API enumeration through the same provider and daemon returns
one `MetaFlux Virtual Compute Device`. The daemon currently routes client
execution to the CPU backend only; the Vulkan backend executes kernels in its
own probes and tests but has no daemon execution mode.

## Scope

Included:

- The provider driver-API surface torch's cudart/ATen initialization needs,
  proven through the pinned baseline client.
- Kernel-intake and module-loading strategy for torch CUDA kernels within the
  decision-0017 epoch rules, including the manifest/corpus revision path when
  torch's PTX forms leave the frozen envelope.
- Eager-operation execution through the daemon: enumeration, copy, kernel
  launch, synchronization, and bit-exact verification against the CPU/torch
  CPU reference.
- Daemon execution routing for framework clients beyond the CPU backend,
  with Vulkan execution as the second target under its own qualification rows.
- Release-facing qualification of the pinned baseline and frontier client
  profiles.

Excluded:

- Vendor-private RM/UVM, NCCL, cuBLAS/cuDNN vendored-binary execution, or any
  promise that torch's closed prebuilt SASS runs without the kernel-intake
  strategy above.
- Physical dual-driver rows, which stay in milestone-2.0.0.0 (decision-0040).
- Any relaxation of the frozen PTX 9.0/`sm_70` oracle without a compiler-epoch
  review.

## Global Acceptance

- The baseline profile reaches probe stage `complete` (import,
  driver-enumeration, runtime-copy, artifact-intake, eager-add all pass)
  against a stock daemon with no client-side patches.
- Eager operations produce results bit-exact against the torch CPU reference
  on the qualified host.
- Every kernel-intake expansion carries its manifest/corpus revision evidence
  and cache-identity bump.
- milestone-0.1.x suites remain green with the framework client installed but
  idle.

## Decisions to Close

1. Kernel-intake strategy for torch kernels: arch-pinned PTX-bearing client
   profiles versus a cubin/SASS intake worker (the deferred 0.1.3.0 research).
2. The exact provider driver-API surface set required by cudart/ATen init.
3. Daemon execution-mode surface for framework clients and its cache identity.
4. Vulkan daemon routing shape and its qualification matrix.

## Definition of Done

milestone-0.2.0.0 is complete when the pinned baseline client passes the full
probe through a stock daemon, the eager-operation corpus is bit-exact, the
daemon routes framework clients to a qualified execution backend (CPU first;
Vulkan when its rows close), frontier-profile gaps are recorded, and the
cumulative 0.1.x regression stays green. Released artifacts follow the
decision-0012 generic package policy.

## References

- [PyTorch compatibility roadmap](../pytorch-compatibility-roadmap.md)
- decision-0017 PTX 9.0/`sm_70` capability and semantic-oracle corpus
- decision-0040 physical-hardware boundary (milestone-2.0.0.0)
