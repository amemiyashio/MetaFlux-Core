---
id: P20260830-009
status: Recorded
captured: 2026-08-30
milestone: M0100
branch: main
git_revision: ded1dad4172b515a3b17cb66c3f7aa18df9cb20e
workspace: agent commits use command-local harness identity; human Git configuration remains unchanged; M0100 product state is unchanged
---

# Agent harness commit identity applied

## Engineering state

Revision `ded1dad4172b515a3b17cb66c3f7aa18df9cb20e` captured the first
`start-work` implementation of agent commit identity. It used a static Codex
and Claude Code table; revision `f862852` later added ZCode. D0028 and SC0004
supersede that mapping authority with automatic runtime-subject derivation.
The original command-local environment and human Git configuration boundary
remains valid.

## Verification evidence

| Gate | Result | Artifact/log |
| --- | --- | --- |
| Isolated harness identity fixture | 7/7 passed | `agent/skills/start-work/scripts/test_commit_as_harness.py` |
| Workflow skill packages | `start-work` and `record-session` both valid | Skill Creator `quick_validate.py` |
| Agent repository gate | 29 sessions, 219 events, 216 Markdown files passed at record closure | `tools/check-agent-records.py` |
| Skill routing | 11 domain skills, 2 workflow skills, 82 cases; self-test 34/34 | Repository routing gates |
| Real commit identity | Author and Committer both `Codex <codex@localhost>` | Content revision `ded1dad` |
| Human identity isolation | Local identity remained `amamiya <amamiya@localhost>` | `git config --local --get-regexp '^user\.(name|email)$'` |

## Decisions and durable outcomes

- At capture time, `start-work` owned static Codex and Claude Code mappings.
  D0028/SC0004 later supersede only that identity-source rule; this checkpoint's
  revisions, identities, gate results, and configuration observation stay
  factual evidence.
- `record-session` references that owner for every agent-created content,
  checkpoint, and closing-record commit.
- The pre-commit hook remains a deterministic repository validator; it does not
  infer whether a human or an agent initiated a commit.

## Open work and risks

- The capture-time conclusion that no identity workflow work remained was
  invalidated when ZCode required another static row. D0028/SC0004 replace that
  route; direct human commits continue to use normal Git configuration.
- Attached short arguments containing `c` or `C` are conservatively rejected by
  the helper; use the documented separated argument form.

## Resume notes

1. Start agent work through `start-work` and keep the active session current.
2. Commit through `commit_as_harness.py`, then verify Author and Committer with
   `git show` before reporting the revision.
3. Under D0028, let the helper read the runtime harness subject automatically;
   missing or ambiguous evidence stops the commit, and the user's Git
   configuration is never rewritten as preparation.
