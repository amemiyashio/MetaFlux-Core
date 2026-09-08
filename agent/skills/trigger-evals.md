# Skill Routing Regression

[`trigger-evals.json`](trigger-evals.json) is the static English/Chinese routing
contract for routed domain skills, automatic harness/CLI tool detection,
implementation-readiness assessment, and the explicit replan, integration,
Epoch-governance, and roast workflows. Each skill has
positive and near-miss coverage; cross-domain work is represented by composition
cases.

`replan-roadmap`, `integrate-batch`, `govern-epoch`, and `roast` are
explicit-only. Every positive case names its exact `$skill-name`; ordinary route
explanation, integration, governance, or roast language remains a near miss.

```sh
nix develop . --command python3 -B tools/check-skill-routing.py .
nix develop . --command python3 -B tools/test-check-skill-routing.py
```

The checks are local and deterministic. They validate the corpus and package
metadata without invoking a remote model or storing run observations.
