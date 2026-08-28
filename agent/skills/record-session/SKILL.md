---
name: record-session
description: Record a work session from scaffold to checkpoint with every machine-enforced rule satisfied on the first pass.
status: Active
---

# Record a Session

Use for any work session that changes durable state. The scaffolder makes the
ceremony cheap; the validator makes skipping it impossible.

## Steps

1. Scaffold before working:
   `python3 tools/new-session.py <slug>` — allocates the id, writes a
   validator-clean skeleton, and registers the index row.
2. Replace the objective TODO in `events.jsonl` and keep appending events
   (`decision`, `tool_call`, `tool_result`, `work_note`) as they happen;
   sequence numbers stay contiguous, one JSON object per line.
3. Fill `session.json`: agents, referenced milestones/work items with honest
   statuses, `base_revision` and `final_revision` from `git rev-parse`
   (never from memory), and `status: complete` when done.
4. Write `summary.md` including the mandatory `## Distillation` section:
   what was promoted into durable records, or `none`. This is where session
   knowledge escapes cold storage.
5. Update the sessions README index row from its scaffolded TODO to the real
   one-line summary.
6. When durable state changed: refresh `progress/current.md` (its `checkpoint:`
   must name the newest checkpoint) and record checkpoint
   `PYYYYMMDD-NNN-<slug>` under `progress/checkpoints/YYYY/` with git
   revisions and verification evidence.
7. Commit content and records as separate commits; the session's
   `final_revision` names the content commit even though the records commit
   comes after.

## Verification

```sh
python3 tools/check-agent-records.py .
```

All rules green — index completeness, distillation, checkpoint freshness,
status consistency — is the proof the session is properly recorded.
