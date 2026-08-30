# Notes

The migration preserved the inventoried raw evidence byte-for-byte. Guidance
G002 and G004 were processed by their target-session owners and removed; no
session-local artifact remains.

## Independent roast forward review

At initial migration content revision
`29a4e38f3a9097ea3680ed7aa98f67ed41105b2e`, an independent read-only
application of D0026 and `agent/skills/roast/SKILL.md` evaluated five forward
fixtures. This was a rule-level review and made no repository change.

| Fixture | Input | Expected | Observed |
| --- | --- | --- | --- |
| A | Three semantically equivalent `timeout-ms` documentation updates target one canonical contract. | Merge them into one `light roasts` claim because the promotion preserves meaning, adds no relation or scope, and updates one owner. | One light claim was produced; no duplicate row was produced for each input. |
| B | Two non-equivalent benchmark observations support only the bounded result `32-64 threads`, `single-NUMA`, `batch-8`. | Record one `medium roasts` synthesis with that explicit boundary; preserve both evidence identities and record evidence maturity separately. | One medium claim was produced with the stated boundary; roast depth did not promote or infer evidence maturity. |
| C | A proposed established-unit replacement changes `ms` to `us`, but no `DNNNN` decision or Active `SCNNNN` exists. | Route the unresolved replacement to its owning plan and `agent/memory/open-decisions.md`; emit no terminal dark-roast or session-only row. | The choice remained unresolved in the plan/open-decision flow and was absent from both roast and session-only. |
| D | The fixture stipulates an existing `/tmp/profile.raw` needed only to resume the current session. | Put the bounded resume claim under `session-only`, and independently account for the actual file under `## Cleanup` with its path, owner, and retention or removal reason. | The review required both records and emitted no roast row. A post-review `test ! -e /tmp/profile.raw` passed, so no fixture artifact remains retained. |
| E | The candidate claim is already equivalent to its canonical owner and causes no material owner update. | Omit it rather than creating a light roast. | No roast or session-only row was produced. |

## Mixed-history diff audit

The evidence audit compared authorization baseline `faf6f6f` with initial
migration content revision `29a4e38`.

- Group A contained exactly 17 authorized files. The sorted multiset of
  7-to-64-character hexadecimal revision and hash tokens extracted from both
  Git trees was byte-identical (`cmp -s`, exit 0). An exact residual scan for
  `distill`, `distillation`, `## Distillation`, `- Promoted:`, and
  `- Session-only:` returned no match.
- Group B contained exactly 14 authorized files. Per-file H2 extraction and
  `cmp -s` found 13/13 verification-family sections byte-identical: 11
  `## Verification` sections and two `## Verification evidence` sections. It
  also found 8/8 `## Cleanup` sections byte-identical.
- The six Historical `Retained evidence` files plus the migration-owner event
  log total seven retained files. This exact comparison returned exit 0:

  ```sh
  git diff --exit-code faf6f6f -- \
    agent/semantic-changes/SC0001-semantic-change-distillation.md \
    agent/sessions/2026/08/S0100-20260828-003-agent-record-convergence/events.jsonl \
    agent/sessions/2026/08/S0100-20260828-004-record-gate-hardening/events.jsonl \
    agent/sessions/2026/08/S0100-20260829-004-implementation-session-current-standard/events.jsonl \
    agent/sessions/2026/08/S0100-20260830-005-semantic-change-distillation/events.jsonl \
    agent/sessions/2026/08/S0100-20260830-005-semantic-change-distillation/session.json \
    agent/sessions/2026/08/S0100-20260830-006-project-knowledge-roast/events.jsonl
  ```

- The independent ownership review then found compound, descriptive, or stale
  owners in 16 migrated summaries. Fifteen follow-up content edits were confined
  to their `## roast` buckets, so their verification, cleanup, session-only, and
  original factual sections remain unchanged. The migration-owner summary's
  ownership row was corrected during record closure. Effective revision
  `991e532` also makes one resolvable repository path or stable ID mandatory for
  every roast row.
