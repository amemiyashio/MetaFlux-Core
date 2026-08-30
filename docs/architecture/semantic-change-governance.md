# Semantic Change Governance

| Field | Value |
| --- | --- |
| Status | Verified |
| Decision | D0025 |
| Applies to | Repository semantics, durable identities, constraints, record contracts, and authority boundaries |

## Decision

A breaking replacement of established repository meaning is governed by one
canonical decision and one independently numbered semantic-change record under
`agent/semantic-changes/`. The current checkout is synchronized to one current
meaning across every affected present and historical record. Git preserves the
prior form; Agent records do not archive another source snapshot.

D0025 supersedes only D0024's statement that its repository-wide rename was the
sole possible historical synchronization exception. D0024 remains the
authoritative product SemVer and delivery-coordinate decision and remains the
first completed pre-framework semantic migration.

## Authorization Boundary

An `SCNNNN` record is a migration permit and durable reminder, not a technical
specification, decision body, source copy, session, or guidance packet. It must
name a resolvable `DNNNN`, bind one in-progress migration session, enumerate the
affected surfaces, and remain `Active` only while the authorized migration is
open.

Terminal sessions and recorded checkpoints are protected history by default.
Their exact paths may change only when they are listed as `Historical` by an
`Active` SC already committed to `HEAD`. Same-commit authorization, unlisted
paths, overlapping active permits, and permits whose migration session is no
longer in progress are rejected. Ordinary corrections append a new correction
or checkpoint instead.

## Evidence Boundary

Historical synchronization may update identifiers, links, terminology, record
shape, and the current interpretation of earlier evidence. It preserves the
earlier observation itself: timestamps, commands, command output, measured
counts, cited/base/final revisions, hashes, provenance, and other factual
evidence are not rewritten.

When new semantics invalidate an earlier conclusion, the current record marks
that conclusion invalidated or pending re-verification. It cannot claim a pass
without new evidence. Old wording may remain only as explicitly inventoried raw
evidence or a deliberately retained compatibility surface.

## Synchronization And Handoff

Activation starts a repository-wide affected-record scan. Every affected path
is resolved as migrated, removed, or retained evidence before the SC becomes
`Applied`; exact residual searches and relevant tests prove that live old/new
meaning is not mixed.

Every affected active session other than the migration owner receives a
transient packet through `session-guidance`. Publishing completes the migration
owner's handoff. The target owner independently validates and dispositions the
packet, records only the compact outcome, and removes the raw packet before its
session becomes terminal.

## Completion And Supersession

`Applied` binds the semantic migration to its content Git revision and closes
its history-edit authority. A later replacement creates a new SC and marks the
older record `Superseded`; identities are never reused or renumbered. The
`govern-semantic-change` skill owns this workflow, while technical domain skills
retain technical meaning and `record-session` retains checkpoint, commit,
cleanup, and session-lifecycle ownership.
