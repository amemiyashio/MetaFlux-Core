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

The existing
[`capabilities.json`](../../../../plugins/compat/cuda/compiler/ptx/manifest/capabilities.json),
[`forms.jsonl`](../../../../plugins/compat/cuda/compiler/ptx/manifest/forms.jsonl)
and [corpus index](../../../../plugins/compat/cuda/compiler/ptx/corpus/index.json)
own the current exact PTX 9.0/sm_70 subset and fixture digests. The initial corpus
is accepted; do not reopen its freeze decision or replace these manifests with
a new draft matrix. The stock PyTorch intake has its own finite dtype/shape
frontier in the active work item.

Start syntax at `kSupportedForms`/`Parser::parse_instruction` in
[`parser.cpp`](../../../../plugins/compat/cuda/compiler/ptx/src/parser.cpp), then
follow the accepted form into the KIR verifier and each advertising backend.
The current contract excludes atomics, warp/cluster forms, z dimensions,
approximate/FTZ/saturating variants, dynamic shared memory and generic/local
addressing. Treat a requested extension as a new exact form with its own
representation and evidence; presence in PTX 9.x alone grants no support.

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

For promotion, update the existing affected manifest rows and fixture digests
with positive and precise negative cases. Preserve directive/version/target,
declaration, parameter-layout, predicate and control-flow diagnostics when a
new arithmetic form is added. Read [semantic family](semantic-family.md) for
representation changes; reuse unrelated accepted rows rather than rebuilding
their evidence during every implementation step.

## Primary source

- [PTX ISA](https://docs.nvidia.com/cuda/parallel-thread-execution/)

Pin the ISA revision used by tests. Do not let a moving documentation revision
expand the advertised repository subset.
