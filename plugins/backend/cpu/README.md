# CPU Backend

The CPU backend is the executable reference backend for the first vertical
slice. It owns Kernel IR execution, CTA scheduling, native lowering, and the
runtime side of the compiled-kernel ABI.

The versioned backend table now exposes the first transport-facing CPU subset:
one device, instance/context/queue handles, caller-owned host-memory import,
synchronous, overlap-safe COPY, and synchronous launch of pre-serialized
canonical Kernel IR. Imported ranges remain owned by the caller; the backend
only retains their address and length until the matching memory handle is
released. A loaded module owns a copy of the canonical KIR, and `submit`
accepts the private `mf_cpu_backend_argument_block_v1` encoding of memory
handles and scalar values. The cdev worker uses this table for mapped-payload
COPY dispatch and now routes the primary-entry launch descriptor through a
generation-bound resolver into the same synchronous submit call. Asynchronous
events, backend DMA mapping, and policy or metrics operations remain outside
this subset until their corresponding transport and lifecycle contracts are
qualified.

Compiler-side and runtime-side code are separate build targets. The compiler
accepts verified canonical Kernel IR v2, emits LLVM-dialect MLIR, translates it
to LLVM IR, runs the LLVM 22 O2 pipeline, emits an x86-64 PIC object, and links a
deterministic `ET_DYN` artifact with `ld.lld`. The helper ABI executes exactly
one explicitly indexed CTA while retaining the complete 2D grid/block shape for
special registers and validation. Logical threads map to inner loops and
barrier-separated regions remain distinct CTA phases. Fused binary32 forms use
a target-independent exact-rounding sequence, so the baseline artifact has no
FMA ISA requirement or imported libm symbol.

Helper ABI v3 (`metaflux_cpu_cta_v3`) additionally takes a fixed scalar
exponential function pointer. The runtime binds `expf` once per process and
retains its library; the identity includes the actual DSO content SHA-256,
resolved function offset and numerical policy. The compiler includes that
identity in cache compatibility and embeds its digest in each closed ELF
object. The loader checks it before exposing the entry. Generated objects have
no undefined symbols or dynamic dependencies; ordinary launches neither resolve
symbols nor hash libraries. `ExpF32` uses the same bound implementation in the
interpreter and compiled paths, with independent accuracy/classification
oracles and bit-exact agreement across CPU modes. It currently uses a scalar
call and has no SIMD performance claim.

The runtime independently validates the ELF class, machine, segment bounds,
non-WX policy, SHA-256 digest, fixed helper ABI, parameter signature, and entry
symbol before `dlopen`/`dlsym`. Launch checks cover dimensions, argument shape,
buffer bounds, alignment, write permission, address overflow, and the supported
floating-point environment. The runtime target has no compiler-core, MLIR,
LLVM, or CUDA dependency.

The interpreter independently decodes canonical Kernel IR v2 and executes the
PTX 9.0/sm_70 manifest as scalar logical threads grouped by 2D CTAs. Each CTA
owns static shared storage, and `bar.sync 0` separates phases so all prior
shared stores are visible before the next phase. The serialized Kernel IR wall
is exercised by differential tests.

The runtime derives a separate dynamic placement profile from process affinity,
online CPUs/nodes, cgroup-v2 effective CPU/memory masks, and sysfs
physical-core/SMT/NUMA topology. It reserves one control core on sets of at
least four cores, runs one pinned worker per remaining physical core, and sends
indivisible interpreter CTAs only to same-node queues. Placement is refreshed
between kernels; explicit pin loss is a stable execution error. Compiled
single-CTA helpers use the same executor queues, so register and static shared
storage are first touched on the pinned worker that owns that CTA.

The O2 lowering recognizes verified barrier-free linear U32 Add/Copy kernels and
emits a canonical lane loop after hoisting buffer bounds and write-permission
checks. It does not assert `noalias`: LLVM emits overlap checks, a scalar
fallback, a vector body, and a scalar tail. Other predication, exact floating
point, shared-memory, and barrier cases retain the generic phase lowering. Tests
inspect optimized LLVM IR and x86 machine code in addition to running the full
interpreter/cold-JIT/warm-JIT/AOT differential corpus.

The independent scalar reference in `compiler/core` defines deterministic
Add/Copy expected values. Corpus tests use separate hard-coded integer and
IEEE-754 bit-pattern oracles; the CPU interpreter remains an implementation
under test rather than the semantic oracle. Native differential tests execute
every advertised form through interpreter, cold JIT, restart-persistent warm
hit, and read-only administrator AOT paths.

The persistent cache uses per-UID and compiler-epoch namespaces, compatibility
keys, bounded reservations, per-UID/global/disk-free quotas, deterministic
unpinned eviction, synchronized temporary publication, digest and metadata
validation, mutable corruption recovery, and a separate read-only AOT tier.
The older in-memory `FixtureArtifactCache` remains explicitly test-only and is
excluded from native JIT/AOT evidence.

Broader SIMD eligibility, SIMD cost crossover evidence, PGO, atomics,
warp/cluster operations, approximate or FTZ floating point, and orchestration of
the generic backend C ABI lifecycle remain outside this slice.
