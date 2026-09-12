# Dialect Conversion

Use this guide when the selected implementation actually uses dialect
conversion. First locate the current emitter or pass in
[target paths](target-lowering.md); a direct CPU LLVM-dialect emission fix does
not require introducing `ConversionTarget`, ODS or a shared high-level dialect.

## Conversion contract

1. Define the `ConversionTarget` before patterns. Mark operations/dialects legal,
   illegal, or dynamically legal with a reason tied to the target contract.
2. Define a `TypeConverter` for every source type and its argument, source,
   target, and optional attribute conversions. Add materializations only where a
   real boundary requires them.
3. Ensure region/block arguments, signatures, calls, returns, branches, and
   successor operands are converted consistently.
4. Prefer conversion patterns that match one semantic responsibility. Use the
   rewriter's remapped operands and replace operations atomically.
5. Choose partial, full, or analysis conversion deliberately. A successful
   partial conversion is insufficient when the pipeline promises no source ops.
6. Fail with a source-located diagnostic when target legality depends on a
   missing feature, limit, memory model, or unsupported semantic form.

## Review traps

- A type is converted in results but not region arguments or attributes.
- An unrealized cast survives to serialization or target translation.
- Dynamic legality reads mutable global target state instead of an explicit
  target environment.
- Pattern benefit accidentally determines semantic behavior.
- A greedy rewrite is used where dialect conversion must prove legality.
- One-to-many lowering loses ownership, alignment, or packed-argument metadata.

Test legal no-op conversion, each rewrite, failed materialization, illegal
leftovers, nested regions, calls, and diagnostic locations.

For each affected pass, state input/output legality, prerequisites, preserved
analyses, deterministic ordering and failure diagnostics. Keep verifier-after-pass
checks in debug/CI and inspect the first invalid intermediate representation.
Preserving an analysis requires proof that its assumptions survive the rewrite;
pass order or pattern benefit must not accidentally decide source semantics.
Select unit/pass/conversion tests for these changes and the target's actual
end-to-end check; formal verification selects covering sets once per phase.

Primary source: [MLIR Dialect Conversion](https://mlir.llvm.org/docs/DialectConversion/).
