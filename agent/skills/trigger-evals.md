# Skill Routing Regression

[`trigger-evals.json`](trigger-evals.json) is the static English/Chinese routing
contract for eleven domain skills and the four workflow entries main, epoch,
batch, and iteration. Main retains identity, readiness, delivery inspection,
publication diagnostics, and knowledge promotion as internal dispatches. Each skill
has positive and near-miss coverage; cross-domain work is represented by
composition cases.

Only epoch is explicit-only. Qualified delivery language routes batch
automatically; explanation requests remain read-only through main. The roster
and invocation policy are loaded from tools/check-agent-state.py by both gates.

```sh
nix develop . --command python3 -B tools/check-skill-routing.py .
nix develop . --command python3 -B tests/architecture/test-check-skill-routing.py
```

The checks are local and deterministic. They validate the corpus and package
metadata without invoking a remote model or storing run observations.
