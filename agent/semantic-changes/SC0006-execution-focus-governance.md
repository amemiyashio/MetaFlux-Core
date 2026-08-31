---
id: SC0006
status: Active
created: 2026-08-31
updated: 2026-08-31
decision: D0029
session: S0100-20260831-045-execution-focus-governance
scope: execution-focus-governance
history_sync: automatic
effective_revision: null
superseded_by: null
---

# SC0006: execution focus governance

## Semantic replacement

- Old meaning: any `in_progress` session present in the candidate index covered
  every durable commit, while cumulative current-progress entries and locally
  available verification paths could compete as de facto scheduling authority.
- New meaning: D0029 establishes one machine-readable execution focus with one
  owner session and one canonical Exit Gate. Each content commit explicitly
  declares and matches that owner; dependency-invalid downstream work and an
  unrelated active session provide no commit authority. Exact record-only close
  and atomic focus handoff paths remain available.
- Authority: D0029 in
  `docs/architecture/execution-focus-governance.md`, approved by the user for
  this engineering-governance adjustment.
- Compatibility consequence: existing commits, checkpoints, and session
  evidence remain unchanged. Existing non-owner active sessions may close or
  await handoff but cannot create new durable content after application.

## Migration inventory

| Surface | Class | Disposition | Evidence |
| --- | --- | --- | --- |
| `AGENTS.md` | Current | Pending | Replace global candidate-session wording with D0029 focus ownership |
| `CLAUDE.md` | Current | Pending | Align bridge summary with exact focus owner and protected history |
| `.claude/README.md` | Current | Pending | Align repository-local bridge description with the focus gate |
| `.claude/hooks/session_start.py` | Tooling | Pending | Banner must route agents through current focus before editing |
| `.claude/hooks/pre_edit.py` | Tooling | Pending | Pre-edit guard must resolve the focus owner instead of any active session |
| `.githooks/pre-commit` | Tooling | Pending | Replace global active-session coverage with exact candidate focus authorization |
| `agent/README.md` | Current | Pending | Add focus to precedence, daily read order, and session authority |
| `agent/memory/constraints.md` | Current | Pending | Record the D0029 single-focus durable constraint |
| `agent/memory/decisions-index.md` | Current | Pending | Resolve D0029 and later mark the applied Verified source |
| `agent/progress/README.md` | Current | Pending | Index machine focus and compact human resume roles |
| `agent/progress/current.md` | Current | Pending | Replace cumulative next-work archive with one target and at most three actions |
| `agent/progress/focus.json` | Current | Pending | New machine-readable owner, mode, Exit Gate, and resume target |
| `agent/sessions/README.md` | Current | Pending | Distinguish active record lifecycle from focus-owned content authority |
| `agent/semantic-changes/README.md` | Current | Pending | Index SC0006 through Active and Applied states |
| `agent/semantic-changes/SC0006-execution-focus-governance.md` | Current | Pending | Resolve inventory, handoffs, evidence locks, and effective revision |
| `agent/skills/start-work/SKILL.md` | Current | Pending | Require focus inspection and explicit session declaration for commits |
| `agent/skills/record-session/SKILL.md` | Current | Pending | Make checkpoint/close and focus handoff ownership explicit |
| `docs/architecture/README.md` | Current | Pending | Resolve D0029 and SC0006 in the architecture index |
| `docs/architecture/execution-focus-governance.md` | Current | Pending | Promote the implemented decision from Proposed to Verified |
| `tools/check-agent-records.py` | Tooling | Pending | Validate focus shape, owner, dependencies, authority, and Exit Gate |
| `tools/test-check-agent-records.py` | Tooling | Pending | Prove unrelated-session rejection, owner commits, closes, and handoffs |
| `tools/new-session.py` | Tooling | Pending | Explain that scaffolding does not itself claim execution focus |
| `tools/README.md` | Current | Pending | Document focus validation and candidate authorization behavior |
| `agent/sessions/2026/08/S0100-20260831-045-execution-focus-governance/session.json` | Active session | Pending | Migration owner lifecycle and final content revision |
| `agent/sessions/2026/08/S0100-20260831-045-execution-focus-governance/events.jsonl` | Active session | Pending | Compact decision, handoff, implementation, and verification outcomes |
| `agent/sessions/2026/08/S0100-20260831-045-execution-focus-governance/summary.md` | Active session | Pending | Cleanup, roast, unresolved boundary, and handoff |
| `agent/sessions/2026/08/S0100-20260831-045-execution-focus-governance/notes.md` | Active session | Pending | Migration inventory and rejected loopholes |
| `agent/progress/checkpoints/2026/P20260828-006-agent-guidance-hardening.md` | Historical | Retained evidence | Preserve the original non-agent change/session-coverage observation byte-for-byte |
| `agent/progress/checkpoints/2026/P20260828-007-claude-bridge.md` | Historical | Retained evidence | Preserve the original Claude bridge behavior and test evidence byte-for-byte |
| `agent/progress/checkpoints/2026/P20260830-013-m0100-closure-consistency.md` | Historical | Retained evidence | Preserve candidate-index gate evidence and revisions byte-for-byte |
| `agent/semantic-changes/SC0005-m0100-closure-record-consistency.md` | Historical | Retained evidence | Preserve the earlier gate correction as factual evidence of its applied meaning |
| `agent/sessions/2026/08/S0100-20260828-006-agent-guidance-hardening/events.jsonl` | Historical | Retained evidence | Preserve observed old gate behavior byte-for-byte |
| `agent/sessions/2026/08/S0100-20260828-006-agent-guidance-hardening/notes.md` | Historical | Retained evidence | Preserve design notes for the superseded coverage rule byte-for-byte |
| `agent/sessions/2026/08/S0100-20260828-006-agent-guidance-hardening/summary.md` | Historical | Retained evidence | Preserve old outcome and verification claims byte-for-byte |
| `agent/sessions/2026/08/S0100-20260828-007-claude-bridge/events.jsonl` | Historical | Retained evidence | Preserve original hook observations byte-for-byte |
| `agent/sessions/2026/08/S0100-20260828-007-claude-bridge/summary.md` | Historical | Retained evidence | Preserve original bridge outcome and verification byte-for-byte |

## Active-session handoff

| Session | Guidance | Status | Outcome |
| --- | --- | --- | --- |
| `S0111-20260831-023-vfio-user-negotiation` | `G001` | Published | Process at the next control boundary; preserve work and stop non-focus content commits |
| `S01110-20260831-034-cdev-object-table-resolver` | `G001` | Published | Process at the next control boundary; preserve work and stop non-focus content commits |
| `S01112-20260831-036-cdev-daemon-object-activation` | `G001` | Published | Process at the next control boundary; preserve work and stop non-focus content commits |
| `S0112-20260830-013-m0110-local-cdev` | `G001` | Published | Process at the next control boundary; preserve work and stop non-focus content commits |
| `S0112-20260831-021-cuda-cdev-add-copy` | `G001` | Published | Process at the next control boundary; preserve work and stop non-focus content commits |
| `S0112-20260831-024-cdev-async-lease` | `G001` | Published | Process at the next control boundary; preserve work and stop non-focus content commits |
| `S0112-20260831-026-cdev-backend-reference` | `G001` | Published | Process at the next control boundary; preserve work and stop non-focus content commits |
| `S0113-20260830-014-m0110-static-vfio-user` | `G001` | Published | Process at the next control boundary; preserve work and stop non-focus content commits |
| `S0113-20260831-022-vfio-user-guest-ring` | `G001` | Published | Process at the next control boundary; preserve work and stop non-focus content commits |
| `S0113-20260831-025-cdev-dma-map` | `G001` | Published | Process at the next control boundary; preserve work and stop non-focus content commits |
| `S0113-20260831-027-cdev-memory-reference` | `G001` | Published | Process at the next control boundary; preserve work and stop non-focus content commits |
| `S0114-20260831-007-transport-fault-qualification` | `G001` | Published | Process at the next control boundary; preserve work and stop non-focus content commits |
| `S0114-20260831-028-cdev-lifecycle-cancel` | `G001` | Published | Process at the next control boundary; preserve work and stop non-focus content commits |
| `S0115-20260831-029-cdev-lifecycle-drain-cancel` | `G001` | Published | Process at the next control boundary; preserve work and stop non-focus content commits |
| `S0116-20260831-030-cdev-multi-region` | `G001` | Published | Process at the next control boundary; preserve work and stop non-focus content commits |
| `S0117-20260831-031-cdev-memory-reference` | `G001` | Published | Process at the next control boundary; preserve work and stop non-focus content commits |
| `S0118-20260831-032-cdev-memory-import` | `G001` | Published | Process at the next control boundary; preserve work and stop non-focus content commits |
| `S0119-20260831-033-cdev-worker-lease` | `G001` | Published | Process at the next control boundary; preserve work and stop non-focus content commits |
| `S0121-20260830-015-m0120-lifecycle-model` | `G001` | Published | Process at the next control boundary; preserve work and stop non-focus content commits |
| `S0122-20260831-001-m0120-existing-transport-lifecycle` | `G001` | Published | Process at the next control boundary; preserve work and stop non-focus content commits |
| `S0123-20260831-008-lifecycle-core-qualification` | `G001` | Published | Process at the next control boundary; preserve work and stop non-focus content commits |
| `S0123-20260831-015-lifecycle-concurrent-authority` | `G001` | Published | Process at the next control boundary; preserve work and stop non-focus content commits |
| `S0131-20260831-002-vulkan-capability-abi` | `G001` | Published | Process at the next control boundary; preserve work and stop non-focus content commits |
| `S0132-20260831-003-vulkan-device-memory` | `G001` | Published | Process at the next control boundary; preserve work and stop non-focus content commits |
| `S0132-20260831-014-vulkan-memory-visibility` | `G001` | Published | Process at the next control boundary; preserve work and stop non-focus content commits |
| `S01322-20260831-037-vulkan-staging-allocation` | `G002` | Published | Process at the next control boundary; preserve work and stop non-focus content commits |
| `S01323-20260831-038-vulkan-device-local-copy` | `G002` | Published | Process at the next control boundary; preserve work and stop non-focus content commits |
| `S0133-20260831-004-vulkan-spirv-lowering` | `G001` | Published | Process at the next control boundary; preserve work and stop non-focus content commits |
| `S0133-20260831-013-vulkan-module-reflection` | `G001` | Published | Process at the next control boundary; preserve work and stop non-focus content commits |
| `S0134-20260831-005-vulkan-execution-streams` | `G001` | Published | Process at the next control boundary; preserve work and stop non-focus content commits |
| `S0134-20260831-010-vulkan-command-recycling` | `G001` | Published | Process at the next control boundary; preserve work and stop non-focus content commits |
| `S0135-20260831-006-vulkan-cache-warm-path` | `G001` | Published | Process at the next control boundary; preserve work and stop non-focus content commits |
| `S0135-20260831-009-vulkan-cache-filesystem` | `G001` | Published | Process at the next control boundary; preserve work and stop non-focus content commits |
| `S0135-20260831-011-vulkan-cache-catalog` | `G001` | Published | Process at the next control boundary; preserve work and stop non-focus content commits |
| `S0135-20260831-012-vulkan-cache-stampede` | `G001` | Published | Process at the next control boundary; preserve work and stop non-focus content commits |

## Evidence preservation

Git preserves every prior commit. The nine Historical rows above remain
byte-for-byte locked: their timestamps, commands, outputs, counts, revisions,
hashes, and statements about the then-current global session gate are factual
evidence. SC0006 changes future authority and current interpretation; it does
not relabel an earlier pass as evidence for D0029.

## Future-agent reminder

Read `agent/progress/focus.json` before selecting work. A session being
`in_progress` records lifecycle only; it does not authorize content. Set
`METAFLUX_SESSION_ID` to the exact focus owner for a content commit. Non-owner
sessions may only use the exact record-only close path, and focus transfer must
close the old owner and install the new owner atomically.

## Verification

| Gate | Result |
| --- | --- |
| User decision authority | Passed: the user explicitly requested engineering governance adjustment after the read-only diagnosis |
| Active-session handoff | Passed at activation boundary: 35 target sessions have published execution-focus packets; the migration owner is excluded |
| Active authorization | Pending activation commit |
| Focus schema and dependency validation | Pending implementation and self-tests |
| Candidate owner and handoff gate | Pending implementation and isolated pre-commit self-tests |
| Claude bridge and workflow routing | Pending migration and focused checks |
| Historical evidence lock and residual search | Pending blob audit and exact old-marker classification |
| Agent records and full relevant tests | Pending |
