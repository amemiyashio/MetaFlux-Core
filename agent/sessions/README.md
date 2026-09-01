# MetaFlux Project Work Sessions

This directory contains compact repository work ledgers for the current
governance epoch. A session records its objective, material decisions and
results, durable changes, cleanup, and bounded resume point. Git owns source
history; sessions do not archive worktree snapshots, build directories,
dependency stores, downloads, routine command output, conversations, user
profiles, or work from another project.

Every retained event must materially change a decision, verification claim, or
handoff. Failed routes are deleted at the session boundary unless one concise
lesson is needed to prevent repetition.

## Current session contract

Sessions use
`sessions/YYYY/MM/S<delivery>-YYYYMMDD-NNN-slug/`. The semantic prefix derives
from `session.json.delivery`; the date and sequence distinguish repeated work
instances. Each directory contains `session.json`, `events.jsonl`, `summary.md`,
and `notes.md`; an `outputs/` directory exists only when an exact acceptance
claim requires bounded bytes with no better canonical owner.

Every current session uses schema version 2 and declares
`governance_epoch: D0029`. An `in_progress` session has `ended_at: null`.
`complete`, `blocked`, and `abandoned` sessions record when work stopped. IDs
and terminal event sequences are immutable outside a decision-authorized
semantic migration. While a session is in progress, its ledger may be curated
and resequenced; Git retains prior committed forms.

Lifecycle is not execution authority. Only the session named by
`agent/progress/focus.json.owner_session` may create durable content, and each
commit declares that identity through `METAFLUX_SESSION_ID`. Scaffolding a
session creates only a ledger. Content authority transfers only through one
record-only commit that terminally settles the old owner, installs one new
current-epoch owner, and updates focus atomically.

## Destructive epoch settlement

SC0007 destructively settled all 82 schema version 1 sessions. Their metadata,
events, notes, summaries, outputs, and transient guidance are absent from the
current tree. The [liquidation tombstone](liquidated-v1.json) contains only
administrative IDs, counts, the source revision, and identities of existing
medium/dark roast owners. It contains no task context, evidence text, result,
resume action, or compatibility route.

Old objectives continue only from current canonical project files in a newly
scaffolded current-epoch session after an atomic focus handoff. A liquidated ID
resolves historical references but can never become a session, focus owner, or
execution input.

## Temporary guidance inbox

An active session may contain a temporary `guidance/` inbox. A specialist
publishes bounded direction as `GNNN-<slug>.ready.md`, optionally with one
candidate `.patch`, without directly editing product source in that role. The
session owner follows
[`session-guidance`](../skills/session-guidance/SKILL.md), validates the input,
and records one material `adopted`, `adapted`, `rejected`, or `deferred`
disposition. A duplicate or obsolete packet is removed as no-material without
an event.

Guidance packets move through `draft`, `ready`, and `processing` only while the
target session is `in_progress`. They are transient coordination inputs, not
session evidence. Remove packets and attachments after resolution. The
pre-commit gate rejects staged additions, modifications, copies, renames, or
type changes under `guidance/` while allowing staged deletion. Terminal
sessions have an empty inbox.

## Knowledge disposition

At checkpoint or close, invoke `$roast` explicitly. The summary classifies each
materially promoted claim into exactly one semantic-transformation depth and
records a compact `claim -> canonical owner (evidence)` row. The independent
`session-only` section records bounded local-retention reasons. Neither section
copies canonical content. Epoch settlement preserves already-promoted medium
and dark claims only in those existing canonical owners; it creates no roast
archive.

## Current index

| Session | Date | Fidelity | Status | Summary |
| --- | --- | --- | --- | --- |
| [S0100-20260831-047-breaking-governance-epoch](2026/08/S0100-20260831-047-breaking-governance-epoch/summary.md) | 2026-08-31 | Exact | Complete | Applied SC0007, liquidated pre-epoch detail, and transferred W0112 to a current-epoch successor |
| [S0112-20260901-001-m0110-w0112-current-epoch](2026/09/S0112-20260901-001-m0110-w0112-current-epoch/summary.md) | 2026-09-01 | Exact | Abandoned | Phase-converged W0112 at P100, then handed focus to destructive SC0008 governance |
| [S0100-20260901-002-agent-startup-resolution](2026/09/S0100-20260901-002-agent-startup-resolution/summary.md) | 2026-09-01 | Exact | Complete | Applied destructive D0031/SC0008 harness and Nix-first startup governance |
| [S0112-20260901-003-m0110-w0112-post-startup-governance](2026/09/S0112-20260901-003-m0110-w0112-post-startup-governance/summary.md) | 2026-09-01 | Exact | Abandoned | Phase-converged unchanged W0112 at P101 and handed focus to SC0009 governance |
| [S0100-20260901-004-host-package-bootstrap](2026/09/S0100-20260901-004-host-package-bootstrap/summary.md) | 2026-09-01 | Exact | Complete | Applied SC0009 and established independent bounded host-privilege governance |
| [S0112-20260901-005-m0110-w0112-post-privilege-governance](2026/09/S0112-20260901-005-m0110-w0112-post-privilege-governance/summary.md) | 2026-09-01 | Exact | In progress | Resuming W0112 after Applied SC0009 with privilege routed through its independent skill |

## Fidelity and retention

Allowed event types are `objective`, `decision`, `tool_call`, `tool_result`, and
`work_note`. Use `fidelity: exact` when project events are captured as they
occur, or `fidelity: reconstructed` when project state is rebuilt from
repository evidence. Reconstruction gaps use a `work_note` with `omitted: true`
and a concrete `reason`.

Inline `content` is UTF-8 and limited to 65,536 bytes. A larger output is stored
as `outputs/NNNN.txt` only when an exact acceptance claim depends on it and no
canonical artifact owns those bytes. Ordinary logs are summarized and removed.
Output references carry exact byte counts and SHA-256 values.

Credentials and unrelated environment data are excluded. Record a sanitized
project action or reference a repository script instead of preserving a
credential-bearing command.

Run the validator from the repository root:

```sh
python3 tools/check-agent-records.py .
```
