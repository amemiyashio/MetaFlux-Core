# Semantic Changes

This directory indexes decision-authorized replacements of established
repository meaning. An SC is a migration permit and future-agent reminder; the
linked canonical decision owns policy, source/tests own behavior, and Git owns
the earlier checkout.

IDs are independent monotonic `SCNNNN` values and are never reused. Allowed
statuses are `Active`, `Applied`, and `Superseded`. Only an `Active` SC already
committed to `HEAD`, bound to an in-progress session, can authorize exact
`Historical` migration rows. See the
[`govern-semantic-change`](../skills/govern-semantic-change/SKILL.md) skill and
[D0025](../../docs/architecture/semantic-change-governance.md).

## Index

| ID | Status | Decision | Scope | Updated |
| --- | --- | --- | --- | --- |
| [SC0001](SC0001-semantic-change-distillation.md) | Applied | D0025 | semantic-change-distillation | 2026-08-30 |
| [SC0002](SC0002-project-knowledge-roast.md) | Applied | D0026 | project-knowledge-roast | 2026-08-30 |
| [SC0003](SC0003-v100-qualification-boundary.md) | Applied | D0027 | v100-qualification-boundary | 2026-08-30 |
| [SC0004](SC0004-automatic-harness-identity.md) | Applied | D0028 | automatic-harness-identity | 2026-08-30 |
| [SC0005](SC0005-m0100-closure-record-consistency.md) | Applied | D0025 | m0100-closure-record-consistency | 2026-08-30 |
| [SC0006](SC0006-execution-focus-governance.md) | Active | D0029 | execution-focus-governance | 2026-08-31 |
