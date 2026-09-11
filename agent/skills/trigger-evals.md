# Skill Routing Regression

[`trigger-evals.json`](trigger-evals.json) is the static English/Chinese routing
contract for eleven domain skills and the four workflow entries
[$main](main/SKILL.md) skill, [$epoch](epoch/SKILL.md) skill,
[$batch](batch/SKILL.md) skill, and [$iteration](iteration/SKILL.md) skill.
The entry controller retains identity, readiness, delivery inspection,
publication diagnostics, and knowledge promotion as internal dispatches. Each skill
has positive and near-miss coverage; cross-domain work is represented by
composition cases.

Only [$epoch](epoch/SKILL.md) skill is explicit-only. Qualified delivery language
routes [$batch](batch/SKILL.md) skill automatically; explanation requests remain
read-only through [$main](main/SKILL.md) skill. The roster
and invocation policy are loaded from tools/check-agent-state.py by both gates.

```sh
nix develop . --ignore-environment --keep HOME --keep USER --command python3 -B tools/check-skill-routing.py .
nix develop . --ignore-environment --keep HOME --keep USER --command python3 -B tests/architecture/test-check-skill-routing.py
```

The checks are local and deterministic. They validate the corpus and package
metadata without invoking a remote model or storing run observations.
Linked invocation cases and Git/execution-concept near misses keep terminology
distinct without changing the machine roster or requiring users to phrase
ordinary requests as skill invocations.
