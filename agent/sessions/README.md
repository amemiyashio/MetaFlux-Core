# MetaFlux Project Work Sessions

This directory contains compact repository work ledgers. A session records its
objective, material decisions and results, durable changes, cleanup, and resume
point. Git owns source history; sessions do not archive worktree snapshots,
build directories, dependency stores, downloads, or routine command output.

It does not archive conversations, user profiles, personal preferences, or work
from another project. Every retained event must materially change a decision,
verification claim, or handoff. Failed routes are deleted at the session
boundary unless one concise lesson is needed to prevent repetition.

Sessions use the date hierarchy
`sessions/YYYY/MM/S<delivery>-YYYYMMDD-NNN-slug/`. The semantic prefix is
derived from `session.json.delivery`; the date and sequence make repeated work
instances unique. The directory contains `session.json`, `events.jsonl`,
`summary.md`, `notes.md`, and an `outputs/` directory. IDs and event sequence
numbers are immutable after a session reaches a terminal status. D0024 was the
first repository-wide migration from the earlier date-only IDs. A later
decision-authorized synchronization follows D0025 and lists each terminal
session surface in an `Active` SC already committed to `HEAD`; factual evidence
remains locked. Outside that workflow, append a correction instead of changing
a terminal ledger. While work is in progress, the ledger may be curated and
resequenced; Git retains its earlier forms.

An `in_progress` session has `ended_at: null`. Complete, blocked, and abandoned
sessions record the date or timestamp when work stopped.

## Temporary guidance inbox

An active session may also contain a temporary `guidance/` inbox. A specialist
publishes bounded direction as `GNNN-<slug>.ready.md` (optionally with a
candidate `.patch` attachment) and does not directly edit product source while
acting in that role. The session owner follows
[`session-guidance`](../skills/session-guidance/SKILL.md), validates the proposal,
and records a material `adopted`, `adapted`, `rejected`, or `deferred`
disposition using the existing event types. Exactly one disposition event
carries both `guidance_id` and `disposition`; `deferred` also carries
`deferred_to`. A duplicate or obsolete packet is resolved as no-material
without an event.

Guidance packets move through `draft`, `ready`, and `processing` only while the
target session is `in_progress`. They are transient coordination inputs, not
session evidence: remove the packet and attachments after resolution, and
retain only a compact result or an independently justified durable record.
Do not stage a raw inbox file; the pre-commit gate rejects added, modified,
copied, renamed, or type-changed `guidance/` paths while allowing their staged
deletion. Terminal sessions must contain no file or symbolic link under
`guidance/`.

At checkpoint or close, invoke `$roast` explicitly. The summary classifies each
materially promoted claim into one semantic-transformation depth and keeps a
compact `claim -> canonical owner (evidence)` row. The independent
`session-only` section records bounded local-retention reasons; neither section
copies canonical content.

| Session | Date | Fidelity | Status | Summary |
| --- | --- | --- | --- | --- |
| [S0100-20260827-001-metaflux-bootstrap](2026/08/S0100-20260827-001-metaflux-bootstrap/summary.md) | 2026-08-27 | Reconstructed | Complete | MetaFlux planning, bootstrap, architecture review, and build hardening |
| [S0100-20260828-001-spec-consistency](2026/08/S0100-20260828-001-spec-consistency/summary.md) | 2026-08-28 | Exact | Complete | Five specification self-review amendments, D0008/D0009, first commits |
| [S0100-20260828-002-layout-convergence](2026/08/S0100-20260828-002-layout-convergence/summary.md) | 2026-08-28 | Exact | Complete | Mesa/Wine-patterned layout convergence, D0010/D0011, component graph gate |
| [S0100-20260828-003-agent-record-convergence](2026/08/S0100-20260828-003-agent-record-convergence/summary.md) | 2026-08-28 | Exact | Complete | Machine-enforced record-loop rules: index completeness, open-decisions ledger, knowledge-summary gate, staleness warnings, session scaffolder |
| [S0100-20260828-004-record-gate-hardening](2026/08/S0100-20260828-004-record-gate-hardening/summary.md) | 2026-08-28 | Exact | Complete | Record gate hardening: current-progress freshness, status-drift warnings, and the fifteen-case validator self-test |
| [S0100-20260828-005-skills-layer](2026/08/S0100-20260828-005-skills-layer/summary.md) | 2026-08-28 | Exact | Complete | Expert-skills layer under agent/skills with three seeds and validator-enforced form |
| [S0100-20260828-006-agent-guidance-hardening](2026/08/S0100-20260828-006-agent-guidance-hardening/summary.md) | 2026-08-28 | Exact | Complete | Guidance hardening: root AGENTS.md, start-work skill, pre-commit session-coverage gate |
| [S0100-20260828-007-claude-bridge](2026/08/S0100-20260828-007-claude-bridge/summary.md) | 2026-08-28 | Exact | Complete | Repository-local Claude Code bridge: @AGENTS.md import, edit-time guards, drift-checked by the validator |
| [S0101-20260828-008-codex-entry-points](2026/08/S0101-20260828-008-codex-entry-points/summary.md) | 2026-08-28 | Exact | Complete | Codex-native AGENTS.md verified and repository entry points enforced by an isolated Nix check |
| [S0101-20260828-009-implementation-readiness-skill](2026/08/S0101-20260828-009-implementation-readiness-skill/summary.md) | 2026-08-28 | Exact | Complete | Codex-native skill packages, enforced discovery compatibility, and implementation-readiness expert guidance |
| [S0100-20260828-010-domain-expert-skills](2026/08/S0100-20260828-010-domain-expert-skills/summary.md) | 2026-08-28 | Exact | Complete | Ten Codex domain experts, source-backed references, composition routing, and trigger evaluations for M0100-M0130 |
| [S0100-20260828-011-skill-design-convergence](2026/08/S0100-20260828-011-skill-design-convergence/summary.md) | 2026-08-28 | Exact | Complete | Sixteen bounded expert skills, runtime-contract ownership, structured routing gates, and lifecycle publication invariants |
| [S0101-20260828-012-ubuntu-2004-glibc-floor](2026/08/S0101-20260828-012-ubuntu-2004-glibc-floor/summary.md) | 2026-08-28 | Exact | Complete | Ubuntu 20.04 LTS and glibc 2.31 fixed as the W0101 userspace release floor |
| [S0100-20260828-013-m0100-foundation](2026/08/S0100-20260828-013-m0100-foundation/summary.md) | 2026-08-28 | Exact | In progress | M0100 registry, compiler/CPU, daemon, and CUDA/NVML foundation implementation |
| [S0101-20260829-001-toolchain-boundary-correction](2026/08/S0101-20260829-001-toolchain-boundary-correction/summary.md) | 2026-08-29 | Exact | Complete | D0022 tool boundary, skill governance, session cleanup, and duplicate-source reclamation |
| [S0100-20260829-002-cleanup-stale-routes](2026/08/S0100-20260829-002-cleanup-stale-routes/summary.md) | 2026-08-29 | Exact | Complete | Remaining D0022 residue corrected; exact temporary and dead-store routes removed |
| [S0101-20260829-003-timezone-mirror-priority](2026/08/S0101-20260829-003-timezone-mirror-priority/summary.md) | 2026-08-29 | Exact | Complete | Explicit current-timezone mirror priority in manage-toolchain routing |
| [S0100-20260829-004-implementation-session-current-standard](2026/08/S0100-20260829-004-implementation-session-current-standard/summary.md) | 2026-08-29 | Exact | Complete | Curated S0100-20260828-013-m0100-foundation ledger and enforced active/terminal session lifecycle semantics |
| [S0100-20260829-005-m0100-vertical-slice](2026/08/S0100-20260829-005-m0100-vertical-slice/summary.md) | 2026-08-29 | Exact | Complete | M0100 CPU-backed vertical slice and Ubuntu 20.04/glibc 2.31 package enforcement |
| [S0100-20260829-006-stage-breakthrough-commits](2026/08/S0100-20260829-006-stage-breakthrough-commits/summary.md) | 2026-08-29 | Exact | Complete | Verified stage-breakthrough trigger with separate content and record commits |
| [S0100-20260829-007-pytorch-cuda-test-tool](2026/08/S0100-20260829-007-pytorch-cuda-test-tool/summary.md) | 2026-08-29 | Exact | Complete | Isolated PyTorch cu126 baseline, cu132 frontier, and staged CUDA gap probe |
| [S0100-20260830-001-m0100-completion-sprint](2026/08/S0100-20260830-001-m0100-completion-sprint/summary.md) | 2026-08-30 | Exact | In progress | TODO: one-line summary |
| [S0101-20260830-002-ubuntu-target-sdk-guide](2026/08/S0101-20260830-002-ubuntu-target-sdk-guide/summary.md) | 2026-08-30 | Exact | Complete | Explicit Ubuntu 20.04 target SDK construction, consumption, and release-gap guide |
| [S0100-20260830-003-session-guidance-loop](2026/08/S0100-20260830-003-session-guidance-loop/summary.md) | 2026-08-30 | Exact | Complete | Session-local specialist guidance with validated disposition and transient cleanup |
| [S0100-20260830-004-semantic-version-line](2026/08/S0100-20260830-004-semantic-version-line/summary.md) | 2026-08-30 | Exact | Complete | Product SemVer, derived M/W/S identities, 25-session migration, and v0.2.0 qualification boundary |
| [S0100-20260830-005-semantic-change-distillation](2026/08/S0100-20260830-005-semantic-change-distillation/summary.md) | 2026-08-30 | Exact | Complete | D0025 governance and SC0001 evidence-preserving prior promotion-model migration applied |
| [S0100-20260830-006-project-knowledge-roast](2026/08/S0100-20260830-006-project-knowledge-roast/summary.md) | 2026-08-30 | Exact | In progress | D0026/SC0002 migration to explicit project-knowledge roast semantics |

## Fidelity and retention

Future project sessions use the same schema. Allowed event types are `objective`,
`decision`, `tool_call`, `tool_result`, and `work_note`. Set `fidelity` to
`exact` when project events are captured as they occur, or `reconstructed` when
an older project state is rebuilt from repository evidence. Reconstruction gaps
use a `work_note` with `omitted: true` and a concrete `reason`.

Inline `content` is UTF-8 and limited to 65,536 bytes. A larger output is stored
as `outputs/NNNN.txt` only when an exact acceptance claim depends on it and no
canonical artifact owns those bytes. Ordinary build/test logs are summarized
and removed. Output references carry exact byte counts and SHA-256 values.

Secrets, credentials, and unrelated environment data are excluded entirely.
Do not preserve even a redacted credential-bearing command; record a sanitized
project action or reference a safe repository script instead.

Run the record validator from the repository root:

```sh
python3 tools/check-agent-records.py .
```
