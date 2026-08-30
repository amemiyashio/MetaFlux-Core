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
| [SC0001](SC0001-semantic-change-distillation.md) | Active | D0025 | semantic-change-distillation | 2026-08-30 |
