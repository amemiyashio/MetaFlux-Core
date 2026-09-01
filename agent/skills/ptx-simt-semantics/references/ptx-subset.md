# PTX Subset

## Manifest dimensions

Represent support at the full instruction-form level:

| Dimension | Examples to distinguish |
| --- | --- |
| Syntax | directive, declaration, instruction, label, operand form |
| Data | scalar/vector, signed/unsigned/bit, integer/FP, width |
| Space | parameter, register, local, shared, global, generic |
| Control | predicate polarity, uniformity assumptions, branch target |
| Memory | cache/order/scope/volatile qualifiers, alignment, vector width |
| Target | PTX version, target feature, address size, capability requirement |
| Oracle | deterministic, allowed-outcome-set, undefined/unsupported |

milestone-0.1.0.0's initial corpus includes entries, parameters, registers, predicates,
required address spaces, 1D/2D thread and block registers, required loads/stores,
move and address arithmetic, integer/basic-FP arithmetic, selected fused forms,
conversions, comparisons, branches, return, and required synchronization. This
list is intent; the canonical manifest must enumerate exact forms before freeze.

## Translation rules

- Preserve source locations and original spelling for diagnostics.
- Resolve declarations, labels, parameter layouts, register classes, and state
  spaces before emitting executable Kernel IR.
- Make implicit PTX conversions, rounding, saturation, flush behavior, and
  address-size effects explicit in Kernel IR or reject them.
- Validate immediates, vector arity, alignment, duplicate declarations, target
  labels, and type/space compatibility without relying on backend diagnostics.
- Unknown or unsupported forms produce structured diagnostics containing stable
  code, source range, PTX form, required capability, and supported alternative
  only when one is semantically exact.

## Primary source

- [PTX ISA](https://docs.nvidia.com/cuda/parallel-thread-execution/)

Pin the ISA revision used by tests. Do not let a moving documentation revision
expand the advertised repository subset.
