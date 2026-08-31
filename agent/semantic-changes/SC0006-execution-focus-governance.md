---
id: SC0006
status: Applied
created: 2026-08-31
updated: 2026-08-31
decision: D0029
session: S0100-20260831-045-execution-focus-governance
scope: execution-focus-governance
history_sync: automatic
effective_revision: 51e0c7551ff8a41e241f952b8bea30e411bdc7d9
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
| `AGENTS.md` | Current | Migrated | `47d5735` requires focus inspection, exact owner declaration, bounded close, and atomic handoff |
| `CLAUDE.md` | Current | Migrated | `47d5735` distinguishes edit routing from candidate-tree commit authority |
| `.claude/README.md` | Current | Migrated | `47d5735` documents the repository-local execution-focus bridge |
| `.claude/hooks/session_start.py` | Tooling | Migrated | `47d5735` banner routes agents through focus before durable work |
| `.claude/hooks/pre_edit.py` | Tooling | Migrated | `47d5735` resolves one exact in-progress focus owner and rejects declared-owner drift |
| `.githooks/pre-commit` | Tooling | Migrated | `47d5735` enforces candidate owner, exact non-owner close, and atomic record-only handoff |
| `agent/README.md` | Current | Migrated | `47d5735` adds focus precedence, directory role, daily read order, and authority boundary |
| `agent/memory/constraints.md` | Current | Migrated | `47d5735` records the D0029 single-focus durable constraint |
| `agent/memory/decisions-index.md` | Current | Migrated | `51e0c75` resolves D0029 to the Verified architecture source |
| `agent/progress/README.md` | Current | Migrated | `47d5735` separates machine focus, compact current projection, and historical checkpoints |
| `agent/progress/current.md` | Current | Migrated | Handoff candidate projects only S0112-046 at M0110/W0112 with three actions |
| `agent/progress/focus.json` | Current | Migrated | Handoff candidate installs product owner S0112-046 and the canonical W0112 Exit Gate |
| `agent/sessions/README.md` | Current | Migrated | `47d5735` separates lifecycle from authority; handoff candidate records old/new owner states |
| `agent/semantic-changes/README.md` | Current | Migrated | Handoff candidate indexes SC0006 as Applied |
| `agent/semantic-changes/SC0006-execution-focus-governance.md` | Current | Migrated | Applied record binds the complete inventory to `51e0c75` and the record-only handoff |
| `agent/skills/start-work/SKILL.md` | Current | Migrated | `47d5735` requires focus inspection, explicit owner declaration, and handoff before redirection |
| `agent/skills/record-session/SKILL.md` | Current | Migrated | `47d5735` bounds checkpoints, closes, compact current state, and focus transfer |
| `docs/architecture/README.md` | Current | Migrated | `ec345f8` indexes D0029 as the execution-focus authority |
| `docs/architecture/execution-focus-governance.md` | Current | Migrated | `51e0c75` promotes the implemented D0029 decision to Verified |
| `tools/check-agent-records.py` | Tooling | Migrated | `47d5735` validates focus shape, owner, dependencies, authority, Exit Gate, and projection |
| `tools/test-check-agent-records.py` | Tooling | Migrated | `47d5735` passes 184 cases covering focus modes, failures, commit paths, and Claude guard |
| `tools/new-session.py` | Tooling | Migrated | `47d5735` reports that scaffolding leaves focus with the existing owner |
| `tools/README.md` | Current | Migrated | `47d5735` documents exact candidate authorization and session scaffolding semantics |
| `agent/sessions/2026/08/S0100-20260831-045-execution-focus-governance/session.json` | Active session | Migrated | Handoff candidate closes the migration owner at final content revision `51e0c75` |
| `agent/sessions/2026/08/S0100-20260831-045-execution-focus-governance/events.jsonl` | Active session | Migrated | Four compact events record objective, D0029, implementation, verification, and handoff |
| `agent/sessions/2026/08/S0100-20260831-045-execution-focus-governance/summary.md` | Active session | Migrated | Terminal summary records cleanup, roast routing, W0112 boundary, and successor command |
| `agent/sessions/2026/08/S0100-20260831-045-execution-focus-governance/notes.md` | Active session | Migrated | Terminal notes record enforced decisions, rejected loopholes, and nine Historical blob IDs |
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
| Active authorization | Passed: activation revision `ec345f85879936391c432d57d76999cf61d5d41f` committed D0029 plus Active SC0006 before behavior changes |
| Focus schema and dependency validation | Passed: `47d5735` and the 184-case self-test cover product/governance modes, owner scope, dependencies, Exit Gate, and current projection |
| Candidate owner and handoff gate | Passed: isolated tests reject unstaged/unrelated/missing owners and content piggyback while accepting exact owner, non-owner close, and atomic handoff |
| Claude bridge and workflow routing | Passed: Claude guard cases, skill-routing 89-case corpus, 34/34 routing self-test, and current workflow residual search |
| Historical evidence lock and residual search | Passed: all nine Historical working blobs match their recorded HEAD object IDs; old global-coverage markers remain only in registered evidence or SC0006 old-meaning prose |
| Agent records and full relevant tests | Passed: checkout/candidate Agent gates, semantic-change 21/21, guidance 20/20, convergence 15/15, commit identity 7/7, and focused architecture CTest 7/7 |
| Atomic product-focus handoff | Passed in the candidate tree: S0100-045 closes, S0112-046 remains in progress at M0110/W0112, and focus/current agree |
