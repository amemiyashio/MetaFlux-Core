---
name: converge-project-changes
description: Audit and converge an explicit current-session or collaborator-delivered change set against MetaFlux project authority, ownership, architecture, records, and verification rules. Use after a durable collaborator handoff or when asked to reconcile cross-contributor changes; do not use for ordinary single-domain review, readiness assessment, temporary advice alone, or decision-authorized semantic migration.
---

# Converge Project Changes

Use this skill after an agent or colleague delivers durable source or record
changes, before the next coherent work unit, and as a fallback before checkpoint
or close. The candidate change set is input to the review, not authoritative
source truth.

Read [the change-set and conformance protocol](references/protocol.md) before
correcting any finding. Use the inventory helper to establish a stable scope:

```sh
python3 -B agent/skills/converge-project-changes/scripts/change_inventory.py \
  --session SESSION_ID
python3 -B agent/skills/converge-project-changes/scripts/change_inventory.py \
  --base BASE_REVISION
```

## Boundary

This skill owns cross-change project conformance and remediation routing. It
does not own technical domain meaning, Git integration, session lifecycle,
knowledge promotion, decision closure, or semantic-migration authority.

- Compose every domain skill materially touched by the change set.
- Use `session-guidance` for findings owned by another `in_progress` session;
  do not edit that session's source or records as its reviewer.
- Use `govern-semantic-change` only for an approved replacement of established
  meaning, identifiers, constraints, record shape, or authority. A compatible
  correction to a candidate change does not create an SC.
- Use `close-decision` only after an open row's evidence condition is met.
- Leave staging, commits, checkpoints, cleanup, and close to `record-session`.
- Invoke `manage-toolchain` or `add-component` when their existing boundaries
  are touched; this skill does not absorb those responsibilities.

Do not infer ownership from Git author or committer identity, paths, timestamps,
processes, agent names, or a dirty worktree. Preserve user and concurrent-session
changes outside the resolved scope.

## Workflow

1. Resolve one explicit change-set identity using the protocol precedence.
   Inventory committed, staged, unstaged, and untracked layers separately. Stop
   and rescan if the helper reports concurrent drift.
2. Establish integration ownership before editing. The current session may
   repair only its own exact increment or a completed change set explicitly
   handed to it. Unknown material remains read-only. Another active session
   receives a transient guidance packet.
3. Load the canonical rules and matching domain skills for touched surfaces.
   Review the candidate against its base revision, the latest user instruction,
   verified source and tests, approved plans, and durable constraints.
4. Report findings first as `blocker`, `required`, or `advisory`. Each finding
   names exact evidence, the violated owner or rule, the responsible owner, and
   one disposition.
5. Repair integration-owned compatible gaps without unrelated refactoring.
   Route foreign ownership, unresolved decisions, and breaking replacements to
   their existing workflows. Re-inventory after each correction boundary.
6. Run focused domain checks plus Agent records and architecture gates at the
   scope justified by the diff. Re-audit the final diff against the same base.

Call a change set `converged` only when its identity remained stable, every
`blocker` and `required` finding is resolved, foreign-owner dispositions are
complete, relevant checks pass, and unknown or concurrent work remains excluded.
Advisory findings never authorize scope expansion.

## Output

Return a temporary review result containing:

1. exact base, end, session or handoff identity, and included change layers;
2. findings ordered by severity, with owner and disposition;
3. compatible corrections actually made;
4. verification commands and results; and
5. remaining risks or the explicit `converged` verdict.

Do not create a governance-report archive, copy a source tree, retain a raw
diff, or add a new record vocabulary. When remediation changes the repository,
record one compact existing session event naming the scope, outcome, and
verification evidence.

## Verification

```sh
python3 -B agent/skills/converge-project-changes/scripts/test_change_inventory.py
python3 -B tools/check-skill-routing.py .
python3 tools/check-agent-records.py .
git diff --check
```

Also run the touched domain tests and the component-graph gate whenever the
change can affect a build boundary or dependency edge.
