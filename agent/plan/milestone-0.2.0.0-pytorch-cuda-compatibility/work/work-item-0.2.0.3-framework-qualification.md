---
id: work-item-0.2.0.3
delivery: 0.2.0.3
milestone: milestone-0.2.0.0
status: Queued
area: compatibility
depends_on: [work-item-0.2.0.2, milestone-0.1.3.0]
updated: 2026-09-10
---

# PyTorch CUDA Vulkan Qualification

## Outcome

The pinned stock PyTorch CUDA profile is qualification-ready: required stream,
event, allocator, synchronization, teardown, and fault semantics pass, and the
same canonical Kernel IR corpus executes through independent CPU/LLVM and
physical AMD RADV/Vulkan daemon routes, using process-level activation without
changing the stock PyTorch wheel or application CUDA calls.

## Implementation Boundary

The optional adapter in `services/metafluxd/src/vulkan_execution.cpp` is an
unqualified integration prototype. Both its build option
`METAFLUX_DAEMON_VULKAN_EXECUTION` and runtime opt-in
`METAFLUX_VULKAN_EXECUTION` are separate from enabling the backend component.
The Vulkan preset builds that component; it does not select a PyTorch GPU route.

The existing adapter prepares CPU code first, accepts missing Vulkan modules,
and reaches CPU fallback or operation-specific daemon branches on some launches.
It also uses a process-shared context, fixed workgroup geometry, partial binding
reflection, a global launch mutex and host staging/copyback. These are concrete
items to reconcile, not a revision of the fixed-backend contract in
[milestone-0.1.3.0](../../milestone-0.1.3.0-vulkan-backend/plan.md).
The route decision stays open; this code's existence does not close it.

## First Qualifying Slice

After the CPU-profile dependency and route decision close, run unchanged stock
PyTorch int32 eager add through copy, artifact intake, GPU dispatch, completion
and readback on a named physical AMD RADV device. Include non-multiple tails,
cross-workgroup sizes and repeated execution. Record exact client/source/build,
device/driver UUIDs, enabled target digest, Kernel IR/SPIR-V identities, and
correlated submit/completion evidence. Prove unavailable GPU, unsupported
lowering and device loss return classified failures without CPU substitution.
This slice does not close the whole work item.

## Work

- [ ] Close the Vulkan daemon routing shape and qualification matrix before
  implementing the qualifying route, reconciling the existing optional adapter.
  Fix backend selection before context resources succeed; specify lifetime,
  cache identity and error ownership without per-kernel CPU fallback.
- [ ] Deliver the first qualifying slice above with physical GPU evidence.
- [ ] Replace fixed launch/argument assumptions with validated geometry and
  complete per-parameter source, scalar and read/write roles. Test unary/copy,
  binary, reductions and irregular dimensions before advertising those forms.
- [ ] Route the work-item-0.2.0.2 corpus through the Vulkan backend without
  changing its neutral request or canonical Kernel IR meaning.
- [ ] Lower accepted Kernel IR through the Vulkan backend's MLIR SPIR-V
  conversion with an explicit target environment, legality checks, reflection,
  and `spirv-val`; do not route Vulkan through LLVM IR.
- [ ] Qualify stream/event ordering, caching-allocator behavior, multi-stream
  concurrency, synchronization, deterministic teardown, and daemon loss
  mid-kernel.
- [ ] Run Vulkan results bit-exact against the CPU-backed run on the qualified
  RADV adapter using the same checked-in corpus manifest. Keep host measurement
  samples and topology/device fingerprints under `tmp/outputs/`.
- [ ] Publish checked-in baseline and frontier gap manifests with exact client,
  provider, daemon, backend, compiler, target-environment, and cache identities.
- [ ] Run the declared corpus in a persistent client process through one
  user-facing activation entry, without test-only daemon orchestration. Verify
  backend selection, socket/render-node access, clean shutdown and loader
  coexistence; release installation/upgrade/removal remain work-item-1.0.0.3.
- [ ] Measure declared workload latency/throughput, memory, copies, cold/warm
  behavior and sustained execution with raw samples and explicit provisional
  thresholds. Standalone FMA throughput is not PyTorch application performance.
- [ ] Before any broader application claim, close the application-scope
  decision in the milestone plan and qualify its exact model/program, execution
  mode, inputs and metrics. No unnamed model or universal training claim is
  implied by completion of the finite corpus.
- [ ] Verify cumulative milestone-0.1.x regression and generic-package policy
  remain green with the framework client installed but idle.

## Exit Gate

The Vulkan route decision is closed; a context keeps its selected backend and
unsupported GPU work never silently executes on CPU. Pinned stock PyTorch runs the same
canonical Kernel IR corpus bit-exact through CPU/LLVM and Vulkan/SPIR-V daemon
routes on the named physical AMD RADV adapter with correlated GPU submission,
completion and readback evidence; all MLIR conversion leaves only target-legal operations; streams,
events, allocator, synchronization, teardown, and daemon-loss rows pass;
process-level activation and sustained corpus execution pass without test-only
orchestration; baseline/frontier gap manifests are current; and the full
milestone-0.1.x regression stays green. An unavailable device or skipped probe
provides no GPU qualification. Intel and physical NVIDIA/dual-driver rows stay
in milestone-2.0.0.0. Any advertised application beyond the corpus additionally
requires its closed scope and recorded end-to-end acceptance.
