# MetaFlux Project Work Sessions

This directory contains repository-local evidence for work performed on
`MetaFlux-Core`. A session may record a project objective, technical decision,
tool call, tool result, work note, changed paths, and verification evidence.

It does not archive conversations, user profiles, personal preferences, or work
from another project. Every event must materially relate to this repository.

Sessions use the date hierarchy
`sessions/YYYY/MM/SYYYYMMDD-NNN-slug/`. The directory contains `session.json`,
`events.jsonl`, `summary.md`, `notes.md`, and an `outputs/` directory. IDs and
event sequence numbers are immutable once published.

| Session | Date | Fidelity | Status | Summary |
| --- | --- | --- | --- | --- |
| [S20260827-001-metaflux-bootstrap](2026/08/S20260827-001-metaflux-bootstrap/summary.md) | 2026-08-27 | Reconstructed | Complete | MetaFlux planning, bootstrap, architecture review, and build hardening |
| [S20260828-001-spec-consistency](2026/08/S20260828-001-spec-consistency/summary.md) | 2026-08-28 | Exact | Complete | Five specification self-review amendments, D0008/D0009, first commits |
| [S20260828-002-layout-convergence](2026/08/S20260828-002-layout-convergence/summary.md) | 2026-08-28 | Exact | Complete | Mesa/Wine-patterned layout convergence, D0010/D0011, component graph gate |
| [S20260828-003-agent-record-convergence](2026/08/S20260828-003-agent-record-convergence/summary.md) | 2026-08-28 | Exact | Complete | Machine-enforced record-loop rules: index completeness, open-decisions ledger, distillation, staleness warnings, session scaffolder |
| [S20260828-004-record-gate-hardening](2026/08/S20260828-004-record-gate-hardening/summary.md) | 2026-08-28 | Exact | Complete | Record gate hardening: current-progress freshness, status-drift warnings, and the fifteen-case validator self-test |
| [S20260828-005-skills-layer](2026/08/S20260828-005-skills-layer/summary.md) | 2026-08-28 | Exact | Complete | Expert-skills layer under agent/skills with three seeds and validator-enforced form |
| [S20260828-006-agent-guidance-hardening](2026/08/S20260828-006-agent-guidance-hardening/summary.md) | 2026-08-28 | Exact | Complete | Guidance hardening: root AGENTS.md, start-work skill, pre-commit session-coverage gate |
| [S20260828-007-claude-bridge](2026/08/S20260828-007-claude-bridge/summary.md) | 2026-08-28 | Exact | Complete | Repository-local Claude Code bridge: @AGENTS.md import, edit-time guards, drift-checked by the validator |
| [S20260828-008-codex-entry-points](2026/08/S20260828-008-codex-entry-points/summary.md) | 2026-08-28 | Exact | Complete | Codex-native AGENTS.md verified and repository entry points enforced by an isolated Nix check |
| [S20260828-009-implementation-readiness-skill](2026/08/S20260828-009-implementation-readiness-skill/summary.md) | 2026-08-28 | Exact | Complete | Codex-native skill packages, enforced discovery compatibility, and implementation-readiness expert guidance |
| [S20260828-010-domain-expert-skills](2026/08/S20260828-010-domain-expert-skills/summary.md) | 2026-08-28 | Exact | Complete | Ten Codex domain experts, source-backed references, composition routing, and trigger evaluations for M0001-M0004 |
| [S20260828-011-skill-design-convergence](2026/08/S20260828-011-skill-design-convergence/summary.md) | 2026-08-28 | Exact | Complete | Sixteen bounded expert skills, runtime-contract ownership, structured routing gates, and lifecycle publication invariants |
| [S20260828-012-ubuntu-2004-glibc-floor](2026/08/S20260828-012-ubuntu-2004-glibc-floor/summary.md) | 2026-08-28 | Exact | Complete | Ubuntu 20.04 LTS and glibc 2.31 fixed as the W01 userspace release floor |

## Fidelity and retention

Future project sessions use the same schema. Allowed event types are `objective`,
`decision`, `tool_call`, `tool_result`, and `work_note`. Set `fidelity` to
`exact` when project events are captured as they occur, or `reconstructed` when
an older project state is rebuilt from repository evidence. Reconstruction gaps
use a `work_note` with `omitted: true` and a concrete `reason`.

Inline `content` is UTF-8 and limited to 65,536 bytes. Larger project command
output is stored as numbered `outputs/NNNN.txt` files and referenced by relative
path, exact byte count, and SHA-256. `outputs/README.md` is only an index and is
never an event output. Output paths may not traverse directories or use symlinks.

Secrets, credentials, and unrelated environment data are excluded entirely.
Do not preserve even a redacted credential-bearing command; record a sanitized
project action or reference a safe repository script instead.

Run the record validator from the repository root:

```sh
python3 tools/check-agent-records.py .
```
