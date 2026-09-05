---
id: work-item-0.1.0.3
delivery: 0.1.0.3
milestone: milestone-0.1.0.0
status: Complete
area: compiler-cpu
depends_on: [work-item-0.1.0.1, work-item-0.1.0.2]
updated: 2026-08-30
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
`v0.1.0` performs no graph fusion in the client path. Cache hits load PIC ELF
without compiler-worker RPC. Compiler epochs use separate namespaces; MLIR bytecode and
LLVM IR are not durable cross-version formats.

The cache key includes toolchain fingerprint, Kernel IR schema, pass pipeline,
target triple, CPU name/canonical features, optimization level, FP semantics,
backend/helper ABIs, PGO ID, and kernel content hash.

## Cache Isolation and Eviction (decision-0014)

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

For milestone-0.1.0.0, an AOT prewarm manifest is the strict per-entry `metadata.v1` file
published beside `kernel.so` under the cache-key digest directory. It binds
format version, compiler epoch, Kernel IR schema, helper ABI, artifact size,
cache key, artifact SHA-256, and backend payload identity. The administrator
prewarm command accepts one PTX input per invocation and atomically publishes
that entry and manifest; repeated invocation is an idempotent hit. This
per-entry contract is the manifest required by the work item, rather than a
separate mutable batch-index format.

## CPU and NUMA Placement (decision-0015)

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

The `f32` `fma.rn` lowering must keep the TwoSum-plus-round-to-odd exact-rounding
sequence over the f64 product: TwoSum recovers the addition residual, then an
inexact even sum is patched by one ulp toward the residual sign (guarded finite),
which makes the final f32 narrowing equal the rounding of the exact value
(Boldo-Melquiond; the finite guard keeps inf/NaN bits untouched). Measured
negative result (2026-09-05): the tempting `fptrunc(f64-fma(fpext, fpext, fpext))`
shortcut double-rounds when the exact result sits on an f32 rounding tie and the
addend is small enough to vanish in the f64 intermediate (for example the
fused-stress vector `0x3f800001 * 0x3fc00000 + 0x80000001`: hardware and the
TwoSum sequence give `0x3fc00001`, the f64 shortcut gives `0x3fc00002`). The
compiled-corpus bit-exact gate catches this class; do not replace the sequence
with the direct f64 FMA.

Measured code-shape result (2026-09-05): per-phase SSA promotion of single-assignment
registers whose definitions are pure, dominate all same-phase readers, and have no
cross-phase readers removes the per-lane array traffic and most active-mask loads
from the generic path (`kCpuPipelineIdentity` gains `ssa-reg-promote-v1`). On the
`cpu_fma_throughput` aot baseline (1M elements x 64 fma.rn, one quiet host) the
median launch fell from 150.3 ms to 2.0 ms and reported throughput rose from
893 MFLOP/s to 67.2 GFLOP/s; part of that ratio is legitimate CSE of this kernel's
16 identical FMA chains once SSA exposes them, so it is an upper bound, not a
generic-kernel claim. The durable evidence is instruction-shape, not the ratio:
scalar f64 sequence operations 448 -> 28, active-mask i8 loads 66 -> 5, register
`vmovss` traffic 182 -> 5. Both PTX and compiled-corpus bit-exact gates stay green;
the TwoSum lowering above is unchanged.

Measured SIMD result (2026-09-06): maximal runs of at least three unpredicated pure
operations inside a phase now emit an eight-lane vector region — the same
TwoSum-plus-nudge sequence over `vector<8xf64>`/`vector<8xi64>` with masked array
transfers at the region boundary (`kCpuPipelineIdentity` gains `simd-region-v1`).
On the same aot baseline the median launch fell further from 2.0 ms to 1.44 ms
(93.3 GFLOP/s nominal) and the generated loop holds only packed f64 arithmetic
(28 `vmulpd`/`vaddpd`/`vsubsd`-class packed forms, zero scalar f64 remains). The
scalar sequence semantics are unchanged per lane; both corpus gates stay green.

Measured result (2026-09-06): stride-one global loads and stores now join the
region — linear construction chains (thread id through the CTA mad, *4 scaling,
address add) are recognized statically, per-group checks replace per-element
gates (bounds via the effective mask, writable once, alignment elided by
construction), and each group uses one masked contiguous transfer
(`kCpuPipelineIdentity` gains `simd-region-mem-v1`; scalar offsets from *4
chains also drop the alignment check). On the same aot baseline the median
launch fell from 1.44 ms to 1.11 ms (120.9 GFLOP/s nominal) with the contiguous
`vmovups` transfers and no gather. Both corpus gates stay green.

Measured result (2026-09-06): the midpoint-nudge is replaced by the
round-to-odd patch over the same TwoSum residual (identity gains
`f32-fma-ro-v2`), and region-invariant buffer loads hoist above the group loop.
On the same aot baseline the median launch fell from 1.11 ms to 1.06 ms
(127.2 GFLOP/s nominal); the RO gain scales with fma density, which this
CSE-collapsed benchmark understates. Both corpus gates stay green, including
the tie and subnormal fixtures.

## PTX Oracle and Corpus (decision-0017)

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
