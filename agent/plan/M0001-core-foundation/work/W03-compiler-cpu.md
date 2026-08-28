---
id: M0001-W03
milestone: M0001
status: Queued
area: compiler-cpu
depends_on: [M0001-W01, M0001-W02]
updated: 2026-08-27
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

The supported PTX corpus is limited to `.entry`, parameters, registers,
predicates, required address spaces, 1D/2D thread/block special registers,
required `ld`, `st`, `mov`, address arithmetic, integer/basic FP `add`, `sub`,
`mul`, required `mad`/`fma`, conversions, comparisons, predicates, `bra`, `ret`,
and required synchronization. Every advertised instruction has parser, verifier,
interpreter, lowering, and differential tests. Unknown or malformed operations
produce stable structured diagnostics.

## Work

- [ ] Define Kernel IR verification and diagnostics; implement the minimal PTX
  lexer/parser and translation.
- [ ] Implement the interpreter before optimized lowering.
- [ ] Implement CPU memory, CTA scheduling, special registers, required barriers,
  deterministic shutdown, scalar Add/Copy reference, and randomized differential
  tests.
- [ ] Implement daemon control lifecycle, credentials, Unix socket activation,
  and isolated compiler workers without a provider `libsystemd` dependency.
- [ ] Implement Kernel IR to MLIR, SIMT-to-loop/SIMD lowering, LLVM IR, PIC ELF,
  helper ABI, cancellation, and resource limits.
- [ ] Implement deterministic keys, atomic cache publication, corruption recovery,
  quota/eviction, epoch isolation, and AOT prewarm manifests.
- [ ] Add LLVM vectorizer reproducers and interpreter/JIT differential tests to
  epoch qualification.

## Exit Gates

The interpreter runs the entire declared corpus and rejects every unknown or
malformed operation. Cold JIT, warm JIT, and AOT produce bit-exact integer and
exact-FP-form Add/Copy results; all other FP forms satisfy their pinned
per-operation oracle or allowed-result set. A cache hit loads the executable
without contacting a compiler worker.
