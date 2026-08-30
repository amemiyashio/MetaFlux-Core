# Semantic Change Record And Migration Protocol

## Record Identity

Records live at `agent/semantic-changes/SCNNNN-slug.md`. IDs are monotonic,
never reused, and stay at their original paths after application or
supersession. The directory README indexes every record in both directions.

Use this frontmatter contract:

```yaml
---
id: SC0001
status: Active
created: YYYY-MM-DD
updated: YYYY-MM-DD
decision: DNNNN
session: S<delivery>-YYYYMMDD-NNN-slug
scope: concise-lowercase-scope
history_sync: automatic
effective_revision: null
superseded_by: null
---
```

Allowed statuses are `Active`, `Applied`, and `Superseded`.

- `Active` requires `effective_revision: null` and permits `Pending` rows.
- `Applied` requires the full 40-character hexadecimal content revision, no `Pending` row, concrete
  evidence for every disposition, and passing verification.
- `Superseded` requires a resolvable successor `SCNNNN`; the successor owns the
  current meaning.

`history_sync` is always `automatic`: activation starts a complete affected-
record scan. It means enforced workflow, not a background daemon or speculative
semantic rewriting.

## Required Sections

Every record has these second-level sections in order: `Semantic replacement`,
`Migration inventory`, `Active-session handoff`, `Evidence preservation`,
`Future-agent reminder`, and `Verification`.

The semantic replacement states old meaning, new meaning, decision authority,
and compatibility consequence. The future-agent reminder is a compact trigger
and invariant, not a duplicate specification.

## Migration Inventory

Use one table with these exact columns:

| Surface | Class | Disposition | Evidence |
| --- | --- | --- | --- |
| `path/to/file` | Current | Pending | Exact search or gate to run |

Allowed classes are `Current`, `Historical`, `Tooling`, and `Active session`.
Allowed dispositions are `Pending`, `Migrated`, `Removed`, and `Retained
evidence`. Every Surface is one exact repository-relative file path. Directories,
patterns, and inferred child coverage are rejected; a reproducible residual
scan is verification evidence, not a substitute for the inventory.

If one file contains both an interpretation that must migrate and factual
evidence that must stay byte-for-byte stable, classify the path as `Migrated`
and name the locked lines or fields in Evidence. Use `Retained evidence` only
when the entire file remains unchanged. For a rename, list both the old and new
exact paths; the old row closes as `Removed` and the new row as `Migrated`.

Another owner's active-session files normally do not enter the inventory. Put
that session in the handoff table and let its owner update the active record. An
`Active session` inventory row is used only when the migration owner owns that
surface or the target owner has returned concrete modification evidence.

`Retained evidence` means old wording remains only because changing it would
alter an observed fact, quoted command/output, count, timestamp, revision, or
hash. It never preserves an obsolete conclusion as current truth.

## Active-Session Handoff

Use one table with these exact columns:

| Session | Guidance | Status | Outcome |
| --- | --- | --- | --- |
| `S...` | `GNNN` | Published | Resolve at the next control boundary |

Allowed statuses are `Published`, `Resolved`, and `Not required`. `Published`
completes the SC author's handoff; the target still claims, dispositions, and
removes the packet before its own terminal transition. Do not edit another
owner's session to manufacture a resolution. Use one `none` row with `Not
required` only when the scan finds no affected recipient other than the
migration owner.

Protected-history file types never change and protected files are never copied.
An authorized rename requires both paths in the same Active SC.

## Verification Boundary

Use a `Gate | Result` table. An applied record contains no placeholder and at
least one explicit passing result. Verification covers canonical and historical
synchronization, old-marker residual searches, locked-evidence review, relevant
domain tests, Agent records, Skill routing, edit hooks, and Git diff gates.

SC verification proves migration integrity. It does not turn historical raw
evidence into proof for a newer revision.
