# Project Knowledge Routing Matrix

Choose one authoritative owner for each minimum independent claim.

| Claim | Authoritative owner | Compact session residue |
| --- | --- | --- |
| Implemented behavior, ABI, layout, or algorithm | Source, tests, contract, subsystem documentation, or Verified architecture | Revision and verification result |
| Stable product direction or maturity | Canonical product source, then compact `memory/project.md` pointer | Source link and change point |
| Durable technical or release constraint | Owning plan, architecture, or toolchain source, then compact `memory/constraints.md` consequence | Decision and canonical link |
| Component ownership or durable terminology | Owning README/contract, then component map or glossary | Reason and link |
| Resolved tradeoff | Owning plan or architecture plus `DNNNN` index row | Decision ID and evidence |
| Unresolved choice | Owning plan plus `memory/open-decisions.md` | Observation and closure condition |
| Release scope, DoD, work split, or dependency | Milestone or work-item plan | Evidence revision |
| Reusable method without reproduced evidence | New or existing `ENNNN Candidate` | Trigger case and evidence gap |
| Reusable method with reproduced evidence | `ENNNN Validated` | Reproducer and result link |
| Current blocker, next action, or resume point | `progress/current.md` | Minimum handoff context |
| Material handoff, release gate, or risk transition | New checkpoint through `record-session` | Checkpoint link |
| Decision-authorized replacement of established meaning | `SCNNNN` through `govern-semantic-change` | SC link and revision |
| Guidance or review advice | First disposition through `session-guidance`; then route adopted/adapted claims here | Guidance ID and disposition |
| Raw benchmark, log, profile, or package evidence | Owning test, benchmark, or release artifact | Command, signal, and artifact link |
| One-off failed route | One concise session note only if it prevents repetition; otherwise discard | Minimal lesson or none |
| Conversation, routine command, or personal preference | Do not persist | none |

## Conflict Rules

- Current source and passing tests outrank summaries.
- Memory points to canonical truth; it does not become a second specification.
- A checkpoint records a material handoff, not every session.
- Experience owns reusable method, not product behavior.
- A Candidate may be useful but is not validated proof.
- A semantic replacement requires an SC even when the text edit looks small;
  ordinary compatible corrections do not.
