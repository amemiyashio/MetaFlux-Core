# MetaFlux Project Agent Context

This directory serves only the `MetaFlux-Core` repository. It preserves project
engineering context without turning Agent notes into a second architecture
specification. It does not store a user profile, personal preferences, general
Agent configuration, cross-project memory, or conversation transcripts.

Product truth remains in source, tests, verified architecture records,
contracts, and approved milestone plans.

## Source precedence

When records disagree, use this order:

1. Current source and passing tests.
2. `Verified` records in [`docs/architecture/`](../docs/architecture/README.md)
   and implemented contracts in [`contracts/`](../contracts/README.md).
3. The active approved milestone under [`plan/`](plan/).
4. Durable summaries under [`memory/`](memory/).
5. Current state and historical checkpoints under [`progress/`](progress/).
6. Validated methods under [`experience/`](experience/).
7. MetaFlux project work records.

`Proposed` architecture and queued plans describe intent, not an implemented
contract. Agent records link to canonical material instead of copying it.

## Directory roles

| Path | Purpose | Mutation rule |
| --- | --- | --- |
| `plan/MNNNN/` | Approved milestone and its work items | Update through an explicit planning decision |
| `memory/` | Stable project, constraints, ownership, terminology, and decision index | Change only when canonical sources change |
| `experience/` | Reusable procedures supported by evidence | Validate before relying on them; supersede instead of silently rewriting conclusions |
| `progress/current.md` | Replaceable resume point | Refresh after material state changes |
| `progress/checkpoints/` | Immutable historical snapshots | Append corrections; never rewrite history |
| `sessions/` | MetaFlux task objectives, technical decisions, commands, results, outputs, notes, and summaries | Keep repository-scoped; do not capture conversations or unrelated context |
| `templates/` | Required record shapes | Keep fields and status vocabularies stable |

## Stable identifiers

- Milestone: `MNNNN`, for example `M0001`.
- Work item: `MNNNN-WNN`, unique within the repository.
- Decision index entry: `DNNNN`.
- Experience: `ENNNN`.
- Checkpoint: `PYYYYMMDD-NNN`.
- Project work record: `SYYYYMMDD-NNN-<slug>`.

Identifiers are never reused, renumbered, or changed after links exist. The file
name starts with the identifier where the record is an instance rather than a
singleton index.

## Status rules

| Record | Allowed statuses |
| --- | --- |
| Milestone | `Draft`, `Queued`, `Active`, `Blocked`, `Complete`, `Superseded` |
| Work item | `Draft`, `Queued`, `Active`, `Blocked`, `Complete`, `Superseded` |
| Experience | `Candidate`, `Validated`, `Superseded` |
| Checkpoint | `Recorded` |
| Project work record | `in_progress`, `complete`, `blocked`, `abandoned` |
| Architecture decision | `Proposed`, `Verified`, `Superseded` |

`Complete` requires the record's acceptance evidence. `Validated` requires a
reproducible command or artifact. `Blocked` names the blocking condition and the
next recheck. `Superseded` links its replacement. A checkpoint is immutable and
does not claim that uncommitted files can be reconstructed.

## Daily read order

Use this fixed order for routine work:

1. This `agent/README.md`.
2. [`memory/README.md`](memory/README.md) and the indexed durable memory.
3. [`progress/current.md`](progress/current.md) and its latest checkpoint when
   historical evidence is needed.
4. The active milestone and relevant workstream under [`plan/`](plan/).
5. Only the related validated records from
   [`experience/`](experience/README.md).

Then inspect Git status and current files before editing. Session records are
project evidence for audits or reconstruction; do not load them by default.
Record new MetaFlux evidence, refresh current progress, and create a checkpoint
at a material handoff boundary.
