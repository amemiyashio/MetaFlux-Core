# Skill Routing Regression

[`trigger-evals.json`](trigger-evals.json) is the static English/Chinese routing
contract for routed domain skills, automatic harness/CLI tool detection,
implementation-readiness assessment, automatic acceptance/advancement, and the
explicit integration, replan, Epoch-governance, and roast workflows. Each skill
has positive and near-miss coverage; cross-domain work is represented by
composition cases.

`integrate-batch`, `replan-roadmap`, `govern-epoch`, and `roast` are
explicit-only. Qualified non-empty delivery language routes
`accept-and-advance` automatically; ordinary route explanation without a
committed delivery or gate evidence remains a near miss.

```sh
nix develop . --command python3 -B tools/check-skill-routing.py .
nix develop . --command python3 -B tests/architecture/test-check-skill-routing.py
```

The checks are local and deterministic. They validate the corpus and package
metadata without invoking a remote model or storing run observations.
