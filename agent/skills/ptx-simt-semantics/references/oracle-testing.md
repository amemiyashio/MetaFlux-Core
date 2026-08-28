# Oracle Testing

## Independence rule

The interpreter is the executable PTX/Kernel IR oracle. Keep its control,
arithmetic, and memory implementation structurally independent from MLIR/LLVM
lowering so one bug is unlikely to reproduce identically. For simple kernels,
also use a scalar mathematical/reference implementation.

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

Record random seeds, PTX and Kernel IR digests, compiler epoch, target features,
FP policy, backend version, and the smallest reproducer. Compare integer results
byte-for-byte; use an explicitly justified FP oracle/tolerance per operation.
Snapshot diagnostic code and salient fields rather than unstable prose alone.
