---
id: work-item-0.3.0.2
delivery: 0.3.0.2
milestone: milestone-0.3.0.0
status: Draft
area: compiler.cpu
depends_on: [work-item-0.3.0.1]
updated: 2026-09-06
---

# Torch Kernel Intake and Eager Execution

## Outcome

torch CUDA kernels reach the managed execution pipeline and eager operations
(add, mul, matmul, reduction) run through the daemon with results bit-exact
against the torch CPU reference.

## Work

- [ ] Close decision item 1: pick the kernel-intake strategy — an
  arch-pinned PTX-bearing client profile (client build emits `compute_70`
  PTX that rides the frozen decision-0017 pipeline) versus the deferred
  cubin/SASS intake worker — and record the governing decision with
  rationale and rejected alternatives.
- [ ] If torch's PTX forms exceed the frozen capability/instruction-form
  manifests, run the decision-0017 manifest/corpus revision with parser,
  verifier, Kernel IR, interpreter, lowering, and differential evidence
  advancing together, plus the compiler-epoch and cache-identity bumps.
- [ ] Load torch modules at corpus volume through the module path: many
  kernels per module, warm-cache hits, and per-kernel function resolution.
- [ ] Execute the eager-operation corpus end-to-end (probe stages
  `artifact-intake` and `eager-add`, then an extended op list) and verify
  bit-exact against the torch CPU reference; archive raw samples under
  `tmp/outputs/`.
- [ ] Keep the frontier profile a recorded gap list: no frontier claim without
  its own intake evidence.

## Exit Gate

The baseline profile reaches probe stage `complete`; the extended eager
corpus is bit-exact through a stock daemon on the CPU backend; any manifest
revision is bound by its own hashes, epoch bump, and green differential
gates.
