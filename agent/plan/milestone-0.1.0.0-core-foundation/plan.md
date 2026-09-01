---
id: milestone-0.1.0.0
delivery: 0.1.0.0
release: v0.1.0
status: Complete
budgets: provisional
depends_on: []
areas: [build, contracts, runtime, compiler, backend.cpu, compat.cuda]
updated: 2026-08-30
---

# milestone-0.1.0.0: Core Foundation

## Outcome

Deliver the first executable MetaFlux vertical slice on Linux x86_64/glibc using
[compiler epoch 1](../../../toolchains/README.md#compiler-epoch-1-decision-0018). An
unmodified CUDA Driver application discovers one managed CPU-backed logical
device, loads embedded PTX, executes Add/Copy through interpreter, JIT, and AOT,
and observes the same device through stock `nvidia-smi`.

The milestone preserves these product constraints:

- Wine-style compatibility, with no new application API, source change, or
  recompilation.
- No compiler framework or C++ runtime in the provider process.
- Cache-hit execution does not contact a compiler worker.
- Warm launch, copy, event, and NVML getters use shared-memory fast paths.
- Managed initialization failure can transfer to a vendor library.
- CUDA, NVML, and later vPCI consume one authoritative registry.

The cross-version control/data-plane ownership model is maintained in
[the architecture record](../../../docs/architecture/control-and-data-plane.md).
milestone-0.1.0.0 establishes the registry, client protocol, and backend ABI required by that
model; it does not implement the milestone-0.1.1.0 kernel/guest transports.

The milestone consumes the repository's canonical
[toolchain declarations](../../../toolchains/README.md). It specifies product
tasks and acceptance evidence, not tool versions or Nix workflow ownership.

## Workstreams

| Workstream | Status | Deliverable |
| --- | --- | --- |
| [work-item-0.1.0.1](work/work-item-0.1.0.1-build-toolchain.md) | Complete | Pinned-tool availability and milestone-0.1.0.0 build/release prerequisite qualification |
| [work-item-0.1.0.2](work/work-item-0.1.0.2-contracts-runtime.md) | Complete | Registry, shared ABI, client protocol, rings, and backend C ABI |
| [work-item-0.1.0.3](work/work-item-0.1.0.3-compiler-cpu.md) | Complete | PTX/Kernel IR oracle, CPU execution, JIT/AOT, and cache |
| [work-item-0.1.0.4](work/work-item-0.1.0.4-cuda-provider.md) | Complete | CUDA Driver ABI provider and managed Add/Copy |
| [work-item-0.1.0.5](work/work-item-0.1.0.5-nvml-provider.md) | Complete | NVML provider and supported stock `nvidia-smi` |
| [work-item-0.1.0.6](work/work-item-0.1.0.6-modes-release.md) | Complete | passthrough/auto/fail-open, provisional performance, and generic packaging |

Every workstream exit gate is mandatory. The dependency metadata in each work
document permits prerequisite preparation and compatible fixture work to run
in parallel without weakening the final ordering.

## Scope

Included:

- Zero/one/multiple registry representations; initial execution uses one CPU
  logical device.
- `managed`, `passthrough`, and `auto` modes.
- CUDA Driver discovery, context, memory, module, launch, stream, event, errors,
  `cuGetProcAddress`, required aliases, and PTDS discovery for Add/Copy.
- Explicit PTX 9.x subset, Kernel IR interpreter, MLIR/LLVM PIC ELF lowering,
  shared AOT/JIT pipeline, CPU CTA/SIMD execution, and content-addressed cache.
- NVML surface for `nvidia-smi -L`, summary, core CSV, `compute-apps`, and required
  `-q/-x` queries.
- Recursion-safe absolute-path CUDA/NVML passthrough.
- Pinned-tool availability plus build, test, packaging, ABI, correctness, and
  performance task foundations.

Excluded:

- vPCI, DKMS, `/dev/nvidia*` aliases, vfio-user, BARs, MSI-X, and hotplug.
- Vulkan, AMDGPU, SPIR-V, cubin/SASS, and `nvdisasm` input.
- CUDA Runtime API, cuBLAS, cuDNN, NVRTC, NCCL, and PyTorch compatibility.
- Distributed execution, TCP transport, placement, and cluster scheduling.
- DCGM, MIG, complete NVIDIA RM/UVM, and all management subcommands.
- Custom allocators, handwritten assembly, Rust, and additional implementation
  languages.

## Acceptance Flow

```text
cuInit
  -> enumerate MetaFlux device
  -> create context and stream
  -> load embedded PTX
  -> allocate host/device buffers
  -> copy input to managed memory
  -> launch Add kernel
  -> record/wait event
  -> copy result to host
  -> validate output
  -> destroy all resources
```

The same unmodified C binary runs against the interpreter, cold JIT miss, warm
JIT hit, and pre-populated AOT cache. Integer and PTX forms whose pinned semantics
require exact floating-point results are bit-exact. Every other floating-point
form is compared with its declared per-operation rounding/FTZ/fusion/approximation
oracle or allowed-result set; there is no milestone-wide generic tolerance.

## Milestone Acceptance

Correctness and ABI:

- Scalar reference, interpreter, JIT, and AOT Add/Copy agree.
- Default, unfiltered CUDA and NVML views captured from the same initial revision
  agree on order and live incarnation. `CUDA_VISIBLE_DEVICES` may filter or
  reorder CUDA only; filtered views correlate common live rows by
  `(UUID, generation)`, not ordinal or BDF alone.
- ABI tests cover exact exports/versions, layouts, short buffers, nulls, repeated
  init/shutdown, concurrency, and per-field errors.
- Provider `DT_NEEDED` has no C++ runtime, LLVM/MLIR, `libatomic`, Python, or
  systemd and is restricted to `libc.so.6` plus `libpthread.so.0` and
  `libdl.so.2` only where the glibc 2.31 floor requires them (decision-0009); providers
  have no project constructor.
- Loading a provider without calling it creates no thread, socket, or heap state.
- C and C++ ABI layout assertions agree on every supported build.

Performance (`v0.1.0` provisional targets):

- Local warm launch: p50 <= 1 microsecond and p99 <= 3 microseconds.
- Uncontended active memfd queue: at most one wake syscall per dispatch, zero
  heap allocation and global lock. The zero-syscall obligation begins with the
  milestone-0.1.1.0 doorbell transports (cdev, vfio-user).
- Kernels >= 100 microseconds: scheduling overhead <= 3%.
- Copies >= 16 MiB: >= 90% of the same-path native baseline with no whole-buffer
  extra copy.
- NVML hot getter p99 <= 5 microseconds; warm MetaFlux/NVML init p99 <= 5 ms.
- `nvidia-smi` at 1 Hz changes compute throughput by <= 0.2%.
- Passthrough throughput loss <= 1%.

All performance results pin CPU affinity and NUMA, record warm-up/sample counts,
separate single-thread/contended cases, archive raw samples with the toolchain
fingerprint, and compare distributions rather than best runs.

These numeric targets remain diagnostic throughout milestone-0.1.0.0 / `v0.1.0`; they do
not become binding release gates from an AMD-only host run. milestone-1.0.0.0 / `v1.0.0`
owns physical NVIDIA H2D/D2H, passthrough, and device-identity evidence and may
promote the budgets to binding only when that complete harness passes.

Release compatibility:

- Generic artifacts contain no required `/nix/store` runtime reference and obey
  the provider glibc symbol ceiling.
- Ubuntu 20.04.6, Ubuntu 22.04.5, Ubuntu 24.04.4, and Rocky Linux 9.8 comprise
  the `v0.1.0` generic matrix. Native NixOS VM/package qualification belongs to
  `v0.2.0`.
- Packages never replace vendor-owned libraries or devices.
- Managed and passthrough files coexist through explicit loader paths or isolated
  namespaces.

## Compiler Link Closure (decision-0019)

The generic daemon links the required MLIR and LLVM component archives into one
static compiler closure. It must not require `libMLIR` or `libLLVM` at runtime.
`METAFLUX_COMPILER_LINK_SHARED_LLVM` remains a qualification-only comparison
switch and is off for release builds.

The comparison used the decision-0018 toolchain, identical 2026-08-29 source, Release
mode with LTO, CPU 0 affinity on the AMD Ryzen 7 H 255 reference candidate, and
a warm filesystem cache. Fresh-process results used 20 warmups and 200 samples
per variant; idle-daemon results used five warmups and 30 samples per variant.

| Measurement | Static components | Private shared MLIR/LLVM |
| --- | ---: | ---: |
| Primary compiler payload | 90,919,584 bytes | 376,267,712 bytes |
| Fresh `--version` p50 / p99 | 5.296 / 8.411 ms | 33.151 / 50.038 ms |
| Socket-ready p50 / p99 | 5.008 / 7.658 ms | 31.368 / 50.752 ms |
| Idle RSS / PSS p50 | 32,032 / 32,004 KiB | 65,648 / 65,636 KiB |
| Idle private-dirty p50 | 1,672 KiB | 7,348 KiB |

The static closure was faster, used less resident and private-dirty memory, and
reduced the primary compiler payload by more than four times. It also removes
two versioned compiler-framework DSOs from the generic runtime dependency set.
Distribution packaging must still remove build-tree/store RUNPATHs and pass the
separate release matrix; this decision does not substitute for those gates.

## Resolved Decisions

| ID | Resolution | Canonical detail | Verification state |
| --- | --- | --- | --- |
| decision-0012 | Freeze the `v0.1.0` four-distribution generic package matrix without raising the glibc 2.31 floor; move native NixOS qualification to `v0.2.0`. | [work-item-0.1.0.1](work/work-item-0.1.0.1-build-toolchain.md#release-qualification-matrix-decision-0012) | Policy verified; provider matrix passed once and complete matrix passed twice. |
| decision-0013 | Discover vendor CUDA/NVML only as one validated, same-build absolute-path pair under distribution whitelists or the root-owned override. | [work-item-0.1.0.6](work/work-item-0.1.0.6-modes-release.md#vendor-library-discovery-decision-0013) | Policy frozen; coexistence fixtures remain a release gate. |
| decision-0014 | Isolate mutable compiler cache content per peer-credential UID with fixed quotas, atomic publication, deterministic eviction, and a separate read-only AOT tier. | [work-item-0.1.0.3](work/work-item-0.1.0.3-compiler-cpu.md#cache-isolation-and-eviction-decision-0014) | Policy frozen; fault and quota tests remain work-item-0.1.0.3 gates. |
| decision-0015 | Derive workers from effective physical cores, keep NUMA-local pools and CTA-granularity work, and disable cross-node stealing by default. | [work-item-0.1.0.3](work/work-item-0.1.0.3-compiler-cpu.md#cpu-and-numa-placement-decision-0015) | Policy and AMD evidence verified; Intel x86_64 support qualification belongs to milestone-1.0.0.0 / `v1.0.0`. |
| decision-0017 | Freeze the compiler-epoch-1 PTX 9.0/sm_70 capability, instruction-form, and semantic-oracle corpus manifests. | [work-item-0.1.0.3](work/work-item-0.1.0.3-compiler-cpu.md#ptx-oracle-and-corpus-decision-0017) | Manifest hashes, positive/rejection coverage, interpreter differential tests, and ordinary/ASan runs verified. |
| decision-0019 | Link the generic daemon to a static MLIR/LLVM component closure; retain shared framework DSOs only as a qualification comparison. | [Compiler link closure](#compiler-link-closure-decision-0019) | AMD cold-process and idle-RSS comparison plus generic distribution packaging verified. |
| decision-0023 | Use AMD x86_64 as the milestone-0.1.0.0 reference host; decision-0027 supersedes only its future Intel destination and assigns Intel x86_64 support qualification to milestone-1.0.0.0 / `v1.0.0`. | [work-item-0.1.0.1](work/work-item-0.1.0.1-build-toolchain.md) | milestone-0.1.0.0 boundary remains frozen; Intel evidence remains future milestone-1.0.0.0 work. |
| decision-0024 | Separate standard three-part product SemVer from four-part delivery coordinates and derived M/W/S identities. | [Release and delivery identity](../../../docs/release-versioning.md) | Repository-wide identity policy. |

## Definition of Done

milestone-0.1.0.0 / `v0.1.0` is complete only when every in-scope workstream exit gate and
milestone acceptance criterion passes, the unmodified CUDA binary runs
Add/Copy, supported stock
`nvidia-smi` sees the same device, all four modes/failure paths are tested, the
release is reproducible from one Git revision and the declared tool identities,
is installable without Nix, archives available provisional benchmark and
compiler fingerprints, and admits no excluded later feature into the `v0.1.0`
hot path. Intel x86_64 support qualification and physical NVIDIA
binding-performance promotion are explicitly outside this DoD and begin with
milestone-1.0.0.0 / `v1.0.0`. Native NixOS VM/package qualification is also outside this
DoD and remains in the unallocated `v0.2.0` support expansion.

[milestone-0.1.1.0](../milestone-0.1.1.0-kernel-guest-transport/plan.md) begins kernel/cdev and static
guest transport work only after this DoD or an explicit milestone-boundary
decision.
