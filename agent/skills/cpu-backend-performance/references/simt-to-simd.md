# SIMT to Loop and SIMD

## Preserve the execution model

Translate one grid into CTAs and one CTA into a scheduling unit with explicit
thread/lane identity, shared storage, barrier phases, active masks, and completion
state. The mapping may use scalar loops, host threads, SIMD vectors, or a hybrid;
none may change PTX-observable semantics. This skill owns the CPU interpreter and
runtime implementation of that mapping; [$ptx-simt-semantics](../../ptx-simt-semantics/SKILL.md) skill supplies the
semantic oracle and expected outcomes.

Start interpreter changes at `execute_kernel_ir`/`execute_cta` in
[`interpreter.cpp`](../../../../plugins/backend/cpu/runtime/src/interpreter.cpp).
Start compiled mappings at `MlirEmitter::operation_phases`, `segment_phase` and
`emit_vector_region` in
[`compiler.cpp`](../../../../plugins/backend/cpu/compiler/src/compiler.cpp).
The current emitter already partitions scalar and vector regions around barrier
phases; it is not a missing generic GPU-to-loop pass. Preserve the same full
block dimensions, CTA-local state and completion semantics in both paths.

## Mapping checklist

- Choose outer grid/CTA loops and inner thread dimensions so index arithmetic is
  explicit and overflow checked.
- Form SIMD packets from lanes only where control, memory, FP, and atomic forms
  are legal. Carry a mask through predication, divergence, tails, and early
  return.
- Use strip mining for runtime trip counts and target vector widths. Test full,
  short, empty, and irregular tails.
- Privatize registers/local state per logical thread. Allocate CTA shared state
  once per CTA and place padding only from measured bank/cache behavior.
- Split execution at CTA barriers into phases or use another model proven not to
  deadlock when workers are oversubscribed. A host barrier among unscheduled
  logical threads is invalid.
- Lower atomics and reductions with the source scope/order intact. SIMD conflict
  handling must preserve per-lane return values where required.
- Keep scheduling policy outside generated semantic identity unless it changes
  helper ABI or code shape; record both in cache and benchmark metadata as
  appropriate.

## Strategy evidence

Compare scalar-loop, fixed-width SIMD, scalable/multiversioned candidates where
the epoch supports them. Report legality, generated instructions, register
pressure/spills, mask cost, memory behavior, and performance distribution. A
faster result on one uniform Add kernel does not justify the strategy for
divergent or barrier-heavy forms.

Choose checks for the changed phase/mask/storage behavior, including scalar
fallback and irregular tails. A performance acceptance still needs the whole
advertised-corpus interpreter/JIT/AOT differential coverage once in its formal
phase. [LLVM vectorization](llvm-vectorization.md) owns codegen proof and exact-FP
tuning; [execution path](execution-path.md) owns the service-to-executor route.
