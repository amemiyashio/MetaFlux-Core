---
id: M0001
legacy_id: "0001"
release: v0.1
status: Active
budgets: provisional
depends_on: []
areas: [build, contracts, runtime, compiler, backend.cpu, compat.cuda]
updated: 2026-08-28
---

# M0001: Core Foundation

## Outcome

Deliver the first executable MetaFlux vertical slice on Linux x86_64/glibc using
compiler epoch 1. Its current descriptor selects stock Clang/LLVM/MLIR/LLD 22.1.8;
the exact downstream correctness patchset and derivation hash remain an open
freeze gate. An
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
M0001 establishes the registry, client protocol, and backend ABI required by that
model; it does not implement the M0002 kernel/guest transports.

## Workstreams

| Workstream | Status | Deliverable |
| --- | --- | --- |
| [M0001-W01](work/W01-build-toolchain.md) | Active | Reproducible build, toolchain epoch, Nix, sysroot, and release closure |
| [M0001-W02](work/W02-contracts-runtime.md) | Queued | Registry, shared ABI, client protocol, rings, and backend C ABI |
| [M0001-W03](work/W03-compiler-cpu.md) | Queued | PTX/Kernel IR oracle, CPU execution, JIT/AOT, and cache |
| [M0001-W04](work/W04-cuda-provider.md) | Queued | CUDA Driver ABI provider and managed Add/Copy |
| [M0001-W05](work/W05-nvml-provider.md) | Queued | NVML provider and supported stock `nvidia-smi` |
| [M0001-W06](work/W06-modes-release.md) | Queued | passthrough/auto/fail-open, performance, and packaging |

Every workstream exit gate is mandatory. The dependency metadata in each work
document permits build/toolchain preparation and compatible fixture work to run
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
- Nix development, CI, packaging, ABI, correctness, and performance foundations.

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
  `libdl.so.2` only where the glibc 2.31 floor requires them (D0009); providers
  have no project constructor.
- Loading a provider without calling it creates no thread, socket, or heap state.
- C and C++ ABI layout assertions agree on every supported build.

Performance (provisional until the M0001-W01 reference-host harness archives
its first baseline; then binding per the front-matter `budgets` status):

- Local warm launch: p50 <= 1 microsecond and p99 <= 3 microseconds.
- Uncontended active memfd queue: at most one wake syscall per dispatch, zero
  heap allocation and global lock. The zero-syscall obligation begins with the
  M0002 doorbell transports (cdev, vfio-user).
- Kernels >= 100 microseconds: scheduling overhead <= 3%.
- Copies >= 16 MiB: >= 90% of the same-path native baseline with no whole-buffer
  extra copy.
- NVML hot getter p99 <= 5 microseconds; warm MetaFlux/NVML init p99 <= 5 ms.
- `nvidia-smi` at 1 Hz changes compute throughput by <= 0.2%.
- Passthrough throughput loss <= 1%.

All performance results pin CPU affinity and NUMA, record warm-up/sample counts,
separate single-thread/contended cases, archive raw samples with the toolchain
fingerprint, and compare distributions rather than best runs.

Release compatibility:

- Generic artifacts contain no required `/nix/store` runtime reference and obey
  the provider glibc symbol ceiling.
- Packages never replace vendor-owned libraries or devices.
- Managed and passthrough files coexist through explicit loader paths or isolated
  namespaces.

## Decisions to Close

These block the indicated implementation and must become durable decisions before
their consumers freeze:

1. Release distribution matrix (glibc baseline closed by D0009).
2. Exact LLVM 22 correctness patchset and derivation hash.
3. Exact PTX corpus and instruction/capability manifest.
4. CUDA/NVML header acquisition and manifest update procedure.
5. Vendor library discovery rules for each supported distribution.
6. Cache root, ownership, quota, eviction, and multi-user isolation.
7. CPU worker topology, NUMA placement, and CTA stealing policy.
8. Private shared LLVM versus static compiler closure for the generic daemon,
   decided from cold-start and RSS measurements.

## Definition of Done

M0001 is complete only when every workstream exit gate and milestone acceptance
criterion passes, the unmodified CUDA binary runs Add/Copy, supported stock
`nvidia-smi` sees the same device, all four modes/failure paths are tested, the
release is reproducible from `flake.lock` and installable without Nix, benchmark
and compiler fingerprints are archived, and no excluded later feature enters the
v0.1 hot path.

[M0002](../M0002-kernel-guest-transport/plan.md) begins kernel/cdev and static
guest transport work only after this DoD or an explicit milestone-boundary
decision.
