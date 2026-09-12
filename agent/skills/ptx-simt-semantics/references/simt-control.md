# SIMT Control

## Current representation first

Read `operation_contract`, `may_be_predicated` and `verify_kernel` in
[`kernel_ir.cpp`](../../../../compiler/core/src/kernel_ir.cpp) before proposing
new control flow. Current Kernel IR is single-assignment, permits only its
bounded final-return branch form, and rejects branch plus `bar.sync` to preserve
unconditional CTA participation. Predication is limited to its admitted store
operations. Keep direct invalid-KIR cases for these constraints; the parser is
not their only ingress.

The model below describes what a requested extension must preserve. It is not
a claim that arbitrary loops, nested reconvergence or conditional barriers are
already represented. New control needs an explicit representation, verifier,
independent oracle and actual backend execution before its support row changes.

## Execution state

Model a grid of CTAs; each CTA owns thread state, shared state, barrier state,
and an execution frontier. A schedulable SIMT unit carries a program counter,
active-lane mask, per-lane predicates/registers, and reconvergence metadata.
Do not encode a fixed hardware warp size into Kernel IR semantics unless the
advertised PTX form normatively requires it.

## Predication and divergence

- Evaluate predicates per lane and suppress all effects for inactive lanes.
- At a divergent branch, partition the current active mask by branch condition,
  preserve both paths, and reconverge according to the selected semantic model.
- Handle nested branches, loops, backedges, early `ret`, and empty masks
  explicitly. A host-language `if` over one representative lane is insufficient.
- Keep scheduler order unobservable except where PTX synchronization or memory
  visibility permits multiple observations. In those cases, specify allowed and
  forbidden outcome sets; randomize legal scheduling to sample them, not to claim
  completeness.

## CTA barriers

- Give each barrier phase an arrival set, expected participants, release rule,
  and memory-order effect.
- Validate barrier IDs/count forms included by the manifest.
- Detect a completed CTA with lanes stranded at an impossible barrier and return
  a stable semantic diagnostic in the interpreter fixture.
- Never lower a CTA barrier to a host thread barrier without proving scheduling
  cannot deadlock when CTAs or lanes are multiplexed.

For changed advertised forms, select all-taken, none-taken, partial-mask,
divergent-return and barrier-phase cases as applicable. An extension admitting
nested divergence or loops also needs nested/backedge reconvergence cases;
unsupported control keeps precise negative cases. Always preserve invalid
conditional-barrier rejection against the pinned
[PTX ISA](https://docs.nvidia.com/cuda/parallel-thread-execution/). Broad future
control coverage is not a reason to rerun unchanged unrelated suites per edit.
