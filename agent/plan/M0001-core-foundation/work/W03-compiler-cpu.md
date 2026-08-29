---
id: M0001-W03
milestone: M0001
status: Active
area: compiler-cpu
depends_on: [M0001-W01, M0001-W02]
updated: 2026-08-29
---

# Compiler, Interpreter, and CPU Backend

## Outcome

Implement one PTX-to-Kernel-IR correctness path and a shared interpreter/JIT/AOT
pipeline targeting PIC ELF on CPU.

## Pipeline and Cache

```text
PTX -> Kernel IR -> interpreter
                 -> MLIR -> LLVM IR -> PIC ELF -> cache -> loader
```

Graph IR describes copy, launch, event, and dependencies after ring consumption;
v0.1 performs no graph fusion in the client path. Cache hits load PIC ELF without
compiler-worker RPC. Compiler epochs use separate namespaces; MLIR bytecode and
LLVM IR are not durable cross-version formats.

The cache key includes toolchain fingerprint, Kernel IR schema, pass pipeline,
target triple, CPU name/canonical features, optimization level, FP semantics,
backend/helper ABIs, PGO ID, and kernel content hash.

## Cache Isolation and Eviction (D0014)

Mutable compiler artifacts live at
`/var/cache/metaflux/compiler/users/<uid>/epoch-<N>/<sha[0:2]>/<sha256>/` under
the system daemon's `0700` ownership. The UID comes only from Unix
`SO_PEERCRED`; request data never selects an owner, and mutable artifacts are
never deduplicated or readable across UIDs. Administrator-prewarmed AOT content
is a separate root-owned, read-only tier at `/var/lib/metaflux/aot/epoch-<N>`.

Defaults are 4 GiB per UID, 32 GiB globally, 256 MiB per entry, and reserved
free space of `max(2 GiB, 5% of the filesystem)`. Temporary files and in-flight
reservations count toward quota. Compilation reserves capacity atomically before
work begins. Publication uses a same-filesystem temporary file, file `fsync`,
atomic rename, and directory `fsync`; every load revalidates metadata, digest,
and ELF shape. Corrupt entries are removed and become misses.

Eviction first considers the requesting UID by oldest successful use with a
digest lexical tie-breaker, then the global oldest eligible entry. Open,
in-flight, and pinned entries are skipped. If no entry can be evicted, the
request returns the stable quota error. Startup reconciliation removes stale
temporaries and recomputes usage. AOT is consulted before the mutable user tier,
and a warm hit never contacts a compiler worker.

For M0001, an AOT prewarm manifest is the strict per-entry `metadata.v1` file
published beside `kernel.so` under the cache-key digest directory. It binds
format version, compiler epoch, Kernel IR schema, helper ABI, artifact size,
cache key, artifact SHA-256, and backend payload identity. The administrator
prewarm command accepts one PTX input per invocation and atomically publishes
that entry and manifest; repeated invocation is an idempotent hit. This
per-entry contract is the manifest required by the work item, rather than a
separate mutable batch-index format.

## CPU and NUMA Placement (D0015)

Effective CPUs are the intersection of `sched_getaffinity`, the kernel online
set, and cgroup v2 `cpuset.cpus.effective`; effective memory nodes also honor
`cpuset.mems.effective`. Capability identity is independent of this transient
placement. Auto mode creates one worker per physical core on the first effective
SMT sibling. With at least four effective physical cores, one whole core is
reserved for control, compiler, and wakeup work; smaller sets reserve none and
never oversubscribe.

Each NUMA node owns a pool. Workers pin to one processing unit, kernel and CTA
memory first-touch that node, and the CTA is the indivisible scheduling unit.
Stealing is allowed only within the same node; cross-node stealing is disabled
unless a later measured mode explicitly enables it. Affinity, cpuset, online, or
hotplug changes take effect only after the current CTA and at a kernel boundary,
where pools quiesce and rebuild. No effective CPU, or loss of an explicit pin,
returns a stable placement error. Cache CPU identity includes canonical codegen
features, not transient affinity or NUMA masks unless they change code shape.

The supported PTX corpus is limited to `.entry`, parameters, registers,
predicates, required address spaces, 1D/2D thread/block special registers,
required `ld`, `st`, `mov`, address arithmetic, integer/basic FP `add`, `sub`,
`mul`, required `mad`/`fma`, conversions, comparisons, predicates, `bra`, `ret`,
and required synchronization. Every advertised instruction has parser, verifier,
interpreter, lowering, and differential tests. Unknown or malformed operations
produce stable structured diagnostics.

## PTX Oracle and Corpus (D0017)

Compiler epoch 1 freezes PTX 9.0 targeting `sm_70`, 64-bit addressing, Kernel
IR schema 2, x/y launch dimensions, 49,152 bytes of static CTA-shared storage,
and unconditional `bar.sync 0`. The authority is the checked-in capability
manifest at SHA-256
`dd520c7c422df27dd844054f362b6490b5d632756070073b56a7bf25df9ac79f`,
the 31-line instruction-form manifest at SHA-256
`6b37afb70ac7f28a53b876ddcbaa622c2d77dd17b35586f700ff135cdc63a79a`,
and the 17-fixture corpus index at SHA-256
`0d0049ee2606dc06f777d4ca2fb674d9f85a4915de6ac868232e6c1bcc2e26bd`.
The corpus aggregate is
`585718e3480f0a9c8e1a21b777ffdd63e8a558f210c89f07e58ca47ed68562fe`.

The oracle requires exact modulo-u32 and explicit IEEE-754 binary32
round-to-nearest-ties-to-even behavior with subnormals preserved. Global and
shared pointers retain distinct provenance. Atomics, warp and cluster forms,
the z dimension, approximate/FTZ/saturating forms, unordered FP comparisons,
dynamic shared memory, generic pointers, and local memory are rejected with
stable diagnostics. Expanding any accepted form or capability requires an
explicit manifest/corpus revision and compiler epoch review; parser,
verification, interpreter, lowering, and differential evidence must advance
together.

## Work

- [x] Define Kernel IR verification and diagnostics; implement the minimal PTX
  lexer/parser and translation.
- [x] Implement the interpreter before optimized lowering.
- [x] Implement CPU memory, CTA scheduling, special registers, required barriers,
  deterministic shutdown, scalar Add/Copy reference, and randomized differential
  tests.
- [x] Implement daemon control lifecycle, credentials, Unix socket activation,
  and isolated compiler workers without a provider `libsystemd` dependency.
- [x] Implement Kernel IR to MLIR, SIMT-to-loop/SIMD lowering, LLVM IR, PIC ELF,
  helper ABI, cancellation, and resource limits.
- [x] Implement deterministic keys, atomic cache publication, corruption recovery,
  quota/eviction, epoch isolation, and AOT prewarm manifests.
- [x] Add LLVM vectorizer reproducers and interpreter/JIT differential tests to
  epoch qualification.

## Exit Gates

The interpreter runs the entire declared corpus and rejects every unknown or
malformed operation. Cold JIT, warm JIT, and AOT produce bit-exact integer and
exact-FP-form Add/Copy results; all other FP forms satisfy their pinned
per-operation oracle or allowed-result set. A cache hit loads the executable
without contacting a compiler worker.
