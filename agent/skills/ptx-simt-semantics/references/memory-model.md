# PTX Memory Model

## Address and state-space model

- Track pointer width, state space, allocation identity, offset, bounds,
  alignment, permissions, context, and generation. Do not collapse pointers to
  unchecked host addresses in Kernel IR.
- Distinguish parameter, local, shared, global, and generic addressing. Define
  generic conversion and aliasing only for forms present in the manifest.
- Check address arithmetic overflow before bounds and preserve byte-level layout
  for host/device copies and packed parameters.
- Define misalignment behavior from the selected PTX form; do not silently fix
  an access by copying through an aligned temporary.

## Visibility and atomics

For every supported load, store, atomic, and fence form, record:

- affected state spaces and widths;
- atomicity and tear behavior;
- order and scope;
- synchronization edge created;
- semantic-oracle relation, CPU interpreter requirement, and target-lowering
  precondition;
- negative diagnostic when a target cannot preserve the semantics.

CTA barrier semantics include both execution convergence and the specified
memory visibility. Host C++ atomics, LLVM atomics, and SPIR-V memory operands are
implementation mechanisms, not the source contract; map them only after the PTX
relation is written down.

## Tests

Select cases for the changed admitted forms: visibility across barrier phases,
stale generation, out-of-bounds, integer overflow, aliases and unsupported
scope/order combinations. When an atomic form is newly admitted, also cover
same-address contention, independent addresses and read-modify-write return
values. The current subset's atomic exclusion remains a negative boundary, not
an instruction to build a new atomic corpus for an unrelated memory change. For each
litmus test, derive the allowed outcome set and named forbidden outcomes from the
pinned model. Use exhaustive finite enumeration or equivalent model evidence when
practical; randomized interpreter schedules sample the set but do not prove it
complete.

Normative details come from the pinned [PTX ISA memory consistency
model](https://docs.nvidia.com/cuda/parallel-thread-execution/).
