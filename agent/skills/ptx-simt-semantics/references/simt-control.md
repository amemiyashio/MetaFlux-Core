# SIMT Control

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

Test all-taken, none-taken, partial masks, nested divergence, divergent return,
loop reconvergence, multiple barrier phases, and invalid conditional barrier
participation against the pinned [PTX ISA](https://docs.nvidia.com/cuda/parallel-thread-execution/).
