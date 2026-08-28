# SIMT to Loop and SIMD

## Preserve the execution model

Translate one grid into CTAs and one CTA into a scheduling unit with explicit
thread/lane identity, shared storage, barrier phases, active masks, and completion
state. The mapping may use scalar loops, host threads, SIMD vectors, or a hybrid;
none may change PTX-observable semantics.

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
