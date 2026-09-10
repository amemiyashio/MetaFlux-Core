# Compiler

The compiler owns MetaFlux Kernel IR and Graph IR, common optimization passes,
cache-key construction, diagnostics, and backend-independent compilation
orchestration.

Kernel IR describes compute kernels: their parameters, per-thread operations,
memory accesses, and synchronization. It is processed by userspace compilers and
execution backends. Linux operating-system drivers live separately under
[`linux-kernel-drivers/`](../linux-kernel-drivers/README.md).

LLVM/MLIR lives in compiler services or workers, never in an application-side
provider. Ecosystem inputs such as PTX belong to their compatibility-layer
plugin, while target lowering belongs to the corresponding execution backend.
The compiler core must remain independent of CUDA, ROCm, Vulkan, and concrete
transport implementations.

Frontend and target selection are independent build roles. For example,
`plugins/compat/cuda/compiler/ptx` depends on compiler core to translate PTX into
Kernel IR, while `plugins/backend/cpu/compiler` lowers that neutral IR to a
validated x86-64 PIC ELF artifact. Future target-lowering paths such as
`plugins/backend/vulkan/compiler` remain separate backend-owned components. A
daemon/compiler-worker package enables the frontends and backends it serves
without enabling application-side provider DSOs.

## Implemented milestone-0.1.0.0 slice

Kernel IR schema v2 is the executable, ecosystem-neutral milestone-0.1.0.0 semantic slice.
It carries typed scalar/buffer parameters, 2D launch values, predicates, exact
integer and binary32 operations, provenance-preserving global/shared addresses,
static shared allocations, guarded stores, and full-CTA barriers. Its verifier
rejects invalid schema, types, indices, definitions, address-space provenance,
predication, and control flow before serialization. In particular, a kernel
cannot combine the bounded final-return branch form with `bar.sync 0`; this
strengthening guarantees unconditional barrier participation in the advertised
subset.

The canonical text form excludes source locations and source register names, so
formatting-only PTX changes retain one semantic content identity while
diagnostics keep their original locations in memory. Schema v1 artifacts are
rejected rather than reinterpreted with v2 semantics.

The core also contains an independent scalar Add/Copy reference and a
length-delimited SHA-256 cache identity covering toolchain fingerprint, compiler
epoch, Kernel IR schema, pass pipeline, target triple, CPU/features,
optimization/FP policy, helper/backend ABIs, PGO ID, and canonical kernel
content. The in-memory `FixtureArtifactCache` remains qualification-only and does
not represent a compiled executable. The production milestone-0.1.0.0 path lowers verified
Kernel IR through MLIR and LLVM to a deterministic x86-64 PIC ELF artifact, then
publishes it through the per-UID, compiler-epoch persistent cache or the separate
read-only administrator AOT tier. The daemon retains publication authority;
cold misses compile in a bounded worker process, while warm and AOT hits load a
validated artifact without contacting that worker.

Each persistent entry carries a strict `metadata.v1` manifest next to
`kernel.so`. It binds the compiler epoch and ABI/schema identities to the cache
key, artifact size, SHA-256, and backend payload. In the administrator tier this
is the milestone-0.1.0.0 AOT prewarm manifest: `metafluxd --prewarm-aot PTX` publishes one
read-only entry atomically, and a repeated invocation resolves the same manifest
as an idempotent cache hit. There is no separate mutable AOT index.
