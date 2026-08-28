# Oracle Testing

## Independence rule

This skill owns the PTX/Kernel IR semantic oracle: normative relations,
deterministic expected values, allowed and forbidden outcome sets, and declared
undefined or unsupported cases. The CPU backend owns interpreter implementation.
Use that interpreter as an executable implementation under test, not as the
source of expected answers, and keep it structurally independent from MLIR/LLVM
lowering. For simple deterministic kernels, also use an independent scalar
mathematical/reference implementation.

## Test layers

1. Parser fixtures preserve source locations and reject malformed tokens,
   declarations, types, spaces, modifiers, and control-flow targets.
2. Verifier fixtures construct invalid Kernel IR directly so parser validation
   cannot mask missing IR invariants.
3. Instruction-form tests cover every manifest row, including edge values,
   signedness, width, FP classifications, predicates, and address boundaries.
4. Interaction tests combine divergence, memory, barriers, and atomics under
   randomized legal schedules.
5. Differential tests compare scalar reference, interpreter, CPU JIT/AOT,
   available native CUDA, and Vulkan only for forms each path advertises.
6. Metamorphic tests vary dead code, register names, block shape, scheduling,
   and equivalent address expressions without changing expected semantics.

## Result rules

- Classify every case as deterministic, allowed-outcome-set, or
  undefined/unsupported. Undefined behavior has no upstream result oracle; assert
  a deterministic rejection only when it is separately labeled as a MetaFlux
  product strengthening.
- Compare deterministic integer results byte-for-byte. Use an explicitly
  justified FP oracle or tolerance per operation.
- For atomics, divergent scheduling, or weak-memory litmus tests, derive the
  allowed and forbidden outcomes from the pinned semantic model. Require every
  observed result to be allowed and assert forbidden outcomes with a finite model,
  exhaustive enumerator, or equivalent argument where practical. Randomized
  scheduling is useful sampling, not proof that the outcome set is complete.

Record random seeds, PTX and Kernel IR digests, compiler epoch, target features,
FP policy, backend version, outcome classification, and the smallest reproducer.
Snapshot diagnostic code and salient fields rather than unstable prose alone.
