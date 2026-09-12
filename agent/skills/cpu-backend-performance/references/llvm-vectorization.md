# LLVM Vectorization

## Follow the generated code

The current CPU compiler emits LLVM-dialect MLIR directly in `MlirEmitter` in
[`compiler.cpp`](../../../../plugins/backend/cpu/compiler/src/compiler.cpp).
Trace the affected KIR opcode through `operation_phases`, `segment_phase`,
`begin_segment_promotions`, `emit_vector_region` and `region_lane_count` to the
LLVM IR and object. Check the scalar region as well as the vector region; a
remark about a different loop is not proof for the requested operation.

For this emitter, established tuning levers are safe SSA promotion, pure vector
regions, then proved stride-one groups and masked memory with group guards.
Apply the lever that matches the observed cost, preserving scalar fallback,
lane bounds, alignment and alias guards. Do not prescribe the whole sequence for
an executor or parser problem. Target-width changes must follow canonical
host-feature dispatch and artifact compatibility rather than ambient native ISA.
SSA promotion requires pure single-assignment definitions and no readers outside
the owning segment. Pure SIMD runs contain unpredicated operations and retain
masked boundary transfers; stride-one memory uses group-level checks. Preserve
array-backed state whenever cross-segment readers still require it.

The exact `fma.rn.f32` expansion uses a binary64 product and TwoSum residual with
round-to-odd before f32 rounding. Preserve that policy, including special values
and subnormals; ordinary multiply/add or broad fast-math is not equivalent.
Changing this expansion or vector legality requires bit-exact full compiled
corpus coverage, independent FMA stress answers and object disassembly. A scalar
host math helper call is not SIMD arithmetic evidence.

## Make legality visible

- Present canonical loops with analyzable induction, bounds, trip counts, and
  exits. Preserve source locations through MLIR/LLVM for optimization remarks.
- Express alignment, aliasing, no-wrap, fast-math, and invariant facts only when
  proved by Kernel IR verification or runtime guards.
- Distinguish loop vectorization from SLP. Inspect both remark streams and the
  final IR/machine code.
- Use runtime alias/alignment checks only when their cost and fallback path are
  measured and semantics remain exact.
- Model masked tails, gathers/scatters, reductions, atomics, calls, and divergent
  control in the cost decision. Do not force vectorization merely to satisfy a
  source-level expectation.

## Epoch qualification

Compiler epoch 1 must carry minimized regressions for any relied-on LLVM 22
behavior, including the repository's loop-vectorizer correctness concerns.
Record exact revision/patchset, command line, target/features, pre-pass IR,
observed wrong or missed behavior, expected result, and upstream issue where
applicable. Run each reproducer through its owning verifier with the declared
epoch tools before promoting an artifact; Nix only materializes those tools.
Keep the declared optimization pipeline, including O2, with its minimized
regressions. Do not replace a diagnosed correctness issue with an undocumented
global optimization disable or infer correctness from a faster microbenchmark.

## Evidence set

- optimization remarks with accepted/rejected reasons;
- LLVM IR around loop metadata and memory operations;
- object disassembly and register/spill observations;
- differential correctness for vector and scalar paths;
- measured crossover points, not one fixed size.

Primary source: [LLVM Auto-Vectorization](https://llvm.org/docs/Vectorizers.html).
