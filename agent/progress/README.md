# Progress Records

[`focus.json`](focus.json) is the machine-readable execution authority under
D0029. It names exactly one in-progress owner and either one dependency-valid
product Exit Gate or one decision-authorized governance migration with a product
resume target. [`current.md`](current.md) is its replaceable human projection:
the same owner and target, the current boundary, explicit blockers, and one to
three next actions. It is not a cumulative work queue.

Historical checkpoints are protected and grouped by year under
`checkpoints/YYYY/`.

Latest checkpoint:
[P20260831-090](checkpoints/2026/P20260831-090-execution-focus-governance.md).

A checkpoint records observed state and verification evidence; it is not a Git
revision unless its metadata names one. Create a checkpoint at a material
handoff, release gate, migration, or before a risky transition. Append a dated
correction by default. D0025 permits synchronization only when the exact path is
listed as `Historical` by a decision-bound `Active` SC already in `HEAD`, with
the original factual evidence preserved. A checkpoint makes a focused increment
recoverable; it does not compete with `focus.json`, bypass an incomplete
milestone dependency, or authorize a new work direction merely because a local
fixture passes.
