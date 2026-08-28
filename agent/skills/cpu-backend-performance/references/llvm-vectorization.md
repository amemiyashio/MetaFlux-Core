# LLVM Vectorization

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
applicable. Run each reproducer in Nix epoch qualification before promoting an
artifact.

## Evidence set

- optimization remarks with accepted/rejected reasons;
- LLVM IR around loop metadata and memory operations;
- object disassembly and register/spill observations;
- differential correctness for vector and scalar paths;
- measured crossover points, not one fixed size.

Primary source: [LLVM Auto-Vectorization](https://llvm.org/docs/Vectorizers.html).
