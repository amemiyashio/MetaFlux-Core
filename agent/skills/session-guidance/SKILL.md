---
name: session-guidance
description: Exchange session-scoped expert direction between collaborators, validate it against current evidence, and dispose of the transient packet without creating an advice archive. Use for mentoring or review guidance that may redirect active work; do not use for ordinary task delegation, direct implementation ownership, or canonical decision closure.
---

# Session Guidance

Use this skill when one collaborator has specialist direction for the agent or
another collaborator who owns an active repository session. The packet is a
temporary coordination object: source, tests, approved plans, and durable
constraints remain authoritative.

Read [the protocol](references/protocol.md) before creating or consuming a
packet. Compose the domain skill that owns the affected technical decision;
this skill owns only the exchange and disposition workflow.

## Author Guidance

1. Identify the target `in_progress` session and state evidence, constraints,
   and expected verification rather than assigning implementation ownership.
2. Scaffold a packet with `scripts/guidance.py create`, complete every required
   section, and keep any candidate diff advisory. A candidate patch belongs
   beside the packet and must not be applied automatically.
3. Publish the completed draft with `scripts/guidance.py publish`. Publishing
   is the handoff boundary; do not edit a `ready` packet in place.

`create` publishes only fully written files. If a hard interruption leaves an
internal temporary file, orphan patch, or structurally malformed draft, inspect
that exact identity and use `scripts/guidance.py recover`; do not bypass normal
packet validation for a valid draft.

```sh
python3 agent/skills/session-guidance/scripts/guidance.py create \
  --session SESSION --slug SLUG --author AGENT_ID --role EXPERT_ROLE \
  --scope SCOPE
python3 agent/skills/session-guidance/scripts/guidance.py publish PACKET.draft.md
```

## Absorb Guidance

1. Scan for published packets when resuming a session, receiving a collaborator
   handoff, entering the next complete work unit, checkpointing, or closing.
   Finish an already-running command before scanning.
2. Claim one packet atomically, then inspect its evidence and any candidate
   patch against current source, tests, the user's latest instruction, and the
   owning domain skill.
3. Classify material guidance as `adopted`, `adapted`, `rejected`, or
   `deferred`. Record the outcome in the target session's existing `decision`
   or `work_note` event with top-level `guidance_id` and `disposition` fields.
4. Resolve the packet only after the script verifies that event. Use the
   explicit no-material route only for a duplicate, obsolete, or empty-signal
   packet whose removal changes no decision, result, or handoff. No-material
   cleanup is rejected once any disposition event names the packet.

```sh
python3 agent/skills/session-guidance/scripts/guidance.py list --session SESSION
python3 agent/skills/session-guidance/scripts/guidance.py claim PACKET.ready.md
python3 agent/skills/session-guidance/scripts/guidance.py resolve \
  PACKET.processing.md --disposition adapted
```

Guidance cannot override current source evidence, a newer user instruction, or
a canonical constraint. Conflicting advice is resolved by evidence and explicit
`supersedes`, not by author count. Escalate a public-contract or irreversible
tradeoff to the existing open-decision workflow.

## Cleanup

Raw packets and candidate patches never enter Git. Delete them through
`resolve`; a terminal session must not retain `draft`, `ready`, `processing`, or
candidate-patch files. Promote only the verified conclusion into the existing
session, decision, progress, or experience records.

## Verification

```sh
python3 agent/skills/session-guidance/scripts/test_guidance.py
python3 tools/check-agent-records.py .
```
