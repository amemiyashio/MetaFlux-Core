# Skill Routing Regression

[`trigger-evals.json`](trigger-evals.json) is the static English routing
contract for seventeen domain skills and ten workflow skills. Workflow ownership
has three groups: the main controller, Epoch/Batch/Iteration orchestration,
and preparation/review/verification/delivery/publication/recovery stages.
The [skill index](README.md) names each entry and its responsibility.

Identity and readiness route to preparation, exact commit inspection to
delivery, transport diagnostics to publication, and knowledge promotion to
review. Each workflow skill has positive and near-miss coverage; linked
orchestration/stage and cross-domain work use composition cases.

Every routed skill has at least three English positive cases and two English
near misses. Every workflow skill also appears in at least two English
composition cases; the corpus has at least twelve composition cases overall.
Prompts remain distinct when translated or added. Schema version 2 retains the
same shape, with English as the only supported locale. The gate rejects other
locale labels and Han text even when a case is labeled English.

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

Expert cases exercise stock library admission/retry, neutral request lifetime,
real metric producers, reusable semantic forms, the actual CPU emitter,
same-path measurement, physical AMD routing and real transport/lifecycle
producers. Separate owners cover the pinned PyTorch profile, cuBLAS library
semantics, compiler subprocess isolation, persistent compiler artifacts,
daemon execution ownership and process activation. Compositions connect worker
cancellation to cache reservations, neutral lifetime fields to daemon teardown,
and stock application activation to its observed profile. Near misses keep
public CUDA ABI, backend execution, package metadata and these new owners apart.
Standalone shader throughput remains distinct from stock PyTorch CUDA throughput,
and Linux kernel code remains distinct from compute Kernel IR.
These declarations check routing coverage; first-action quality also needs an
independent task walkthrough against the skill and current source, reported in
conversation rather than stored as another progress or review archive.
