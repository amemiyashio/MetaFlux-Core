# MetaFlux Project Agent Context

## Before changing anything

1. If a task matches an [expert skill](skills/README.md), follow it verbatim.
2. Scaffold a session first for durable work: `python3 tools/new-session.py
   <slug>`. A session is a curated ledger and cleanup boundary, not a snapshot.
3. Never relax a durable constraint in [`memory/constraints.md`](memory/constraints.md)
   without a recorded decision.

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
| `progress/checkpoints/` | Immutable historical handoffs | Append corrections; never rewrite history |
| `sessions/` | Curated task objective, material decisions/results, cleanup, and resume summary | Keep compact; Git owns source history, and disposable failed-route artifacts are removed at handoff |
| `skills/` | Codex skill packages for repository-specific work | Load on demand; keep `SKILL.md` standard-compatible and verify repository-changing procedures proportionately |
| `templates/` | Required record shapes | Keep fields and status vocabularies stable |

## Session-local guidance

An active session may temporarily receive specialist direction or a candidate
patch under its `guidance/` directory. This is an inbox, not another evidence or
source archive. Do not load its contents as part of the daily read order and do
not load the [`session-guidance`](skills/session-guidance/SKILL.md) skill unless
a ready packet exists or the user explicitly asks to publish or process
guidance.

Check the active session for ready guidance when resuming it, after a specialist
or colleague completion notice, before starting the next coherent work unit,
and before checkpoint or close. A long-running command need not be interrupted;
use its next control boundary. While acting as a guidance author, a specialist
may provide evidence, direction, and a candidate patch but does not edit product
source. The session owner claims the packet, validates it against current source,
tests, the user's latest request, and canonical constraints, then records an
`adopted`, `adapted`, `rejected`, or `deferred` disposition. Exactly one existing
`work_note` or `decision` event carries both `guidance_id` and `disposition`; a
deferred result also carries `deferred_to` naming the durable unresolved-work or
open-decision target. A duplicate or obsolete packet may instead be resolved as
no-material with a reason and no session event.

Remove the raw packet and any attachment after resolution. Preserve only the
compact material outcome in the existing session event vocabulary or promote it
to the existing decision, progress, or experience records when their normal
criteria apply. A terminal session's guidance inbox contains no file or
symbolic link.

## Stable identifiers

- Milestone: `MNNNN`, for example `M0001`.
- Work item: `MNNNN-WNN`, unique within the repository.
- Decision index entry: `DNNNN`.
- Experience: `ENNNN`.
- Checkpoint: `PYYYYMMDD-NNN`.
- Project work record: `SYYYYMMDD-NNN-<slug>`.
- Skill: a durable lowercase-hyphenated slug naming one `skills/<slug>/`
  directory; slugs are never renamed after links exist.

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
| Skill catalog entry | `Draft`, `Active`, `Retired` |
| Performance budgets | `provisional`, `binding` |

`Complete` requires the record's acceptance evidence. `Validated` requires a
reproducible command or artifact. `Blocked` names the blocking condition and the
next recheck. `Superseded` links its replacement. A checkpoint is immutable and
does not claim that uncommitted files can be reconstructed. A numeric
performance budget is `provisional` until the measurement harness it names
exists and a baseline is archived; provisional budgets guide design but do not
fail acceptance. Session summaries recorded from 2026-08-28 onward require a
`Distillation` section stating what was promoted into durable records (`none`
is valid). Sessions from 2026-08-29 onward also require a `Cleanup` section
naming removed and intentionally retained session-owned artifacts. Unresolved decisions are aggregated in
[`memory/open-decisions.md`](memory/open-decisions.md) and scaffold new sessions
with `tools/new-session.py`.

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
For an active session, inspect only whether its guidance inbox has a ready packet
at the control boundaries above and load the packet on demand. Record new
MetaFlux evidence, refresh current progress, and create a checkpoint at a
material handoff boundary.
