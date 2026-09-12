# Controller Interface

The executable remains `agent/skills/main/scripts/main.py --root .`.
Every application invocation uses the clean Nix entry in
[$main](../SKILL.md#bootstrap) skill. Shared Python mechanisms live in `agent/lib/`.

| Operation | Owner and input |
| --- | --- |
| `inspect` | Read-only action card; never replaces active work |
| `begin --request-json JSON` | [$prepare](../../prepare/SKILL.md) skill's bounded request |
| `load-rules [--for ACTION] [--skill NAME]` | Emit complete action modules and bind actual versions |
| `step prepared` | Preparation → implementation |
| `step review --payload-json JSON` | [$review](../../review/SKILL.md) skill's parent conclusion → evaluation |
| `preflight`, `step evaluate` | [$verify](../../verify/SKILL.md) skill's actual reviewed plan → delivery |
| `step deliver --payload-json JSON` | [$deliver](../../deliver/SKILL.md) skill's exact guarded commit |
| `step publish` | [$publish](../../publish/SKILL.md) skill's exact commit → handoff |
| `step handoff --payload-json JSON` | Record exact assignment request; complete this operation |
| `resume`, `rescope`, `supersede`, `step repair` | [$recover](../../recover/SKILL.md) skill |

Load the card's current modules before its action. Before parent review, use
`load-rules --for review` while the state is implementation. Integration uses
the Batch wrapper's combined review/verify module and exact delivery base.
The action selector does not advance state, permit an illegal event or confer
authorization. The controller validates the actual event separately.

The schema-2 rule certificate binds action, workflow, request/run, full HEAD,
operation or integration base, selected skills and exact module modes/blobs.
The complete module set is derived from `agent/lib/stage_rules.py`; action cards,
the loader and hooks share it. References declared for that action are emitted
in full. Conditional deep references remain discoverable from the owning skill;
no arbitrary excerpt or truncated body qualifies as a complete module.

Schema-3 verification receipts retain immutable review and verification
certificates, tested input/plan/toolchain and actual execution evidence.
Changing the current stage certificate does not invalidate unchanged tested
bytes. Both historical certificates are checked against the tested tree;
delivery also checks its own current request/base/HEAD-bound certificate.
Changed tested content, rule versions, modes, plan or toolchain require their
normal review/verification recovery. See
[verification](../../verify/references/verification.md).

Current operation/certificates/receipts/transactions remain ignored under
`agent/tmp/main/`. Goal alone owns product route and accepted progress.
[Tool hooks](tool-hooks.md) cover supported calls only when their exact host
definition is trusted. They emit required modules before retrying a blocked call;
the repository neither activates that trust nor proves model comprehension.
