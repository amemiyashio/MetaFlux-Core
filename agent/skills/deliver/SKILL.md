---
name: deliver
description: Commit the exact reviewed and verified MetaFlux tree through the guarded helper and emit a candidate or activation delivery without repeating completed verification.
---

# Deliver

Role: candidate-delivery parent, Batch integrator, or Epoch governor.
Input: current delivery state and its actual content-bound verification receipt.
First action: `main.py load-rules` for delivery, then inspect and stage the exact
reviewed paths through the clean Nix entry.

Run `main.py step deliver --payload-json PAYLOAD_JSON`, with `agent_tool` set
to the conversation-emitted harness and `message` a plain commit message.
The helper in `scripts/commit_as_agent_tool.py` binds expected HEAD, exact
staged tree, operation kind, actual receipt and current delivery rule certificate.
Its hook validates the same evidence before and after candidate state/routing.
Neither helper nor hook changes Git identity configuration or reruns self-tests.

If only staging is missing, stage the reported reviewed paths and retry this
same delivery. Do not use resume, roll back acceptance or rerun tests for
unchanged staging. Changed bytes/modes, HEAD, plan or toolchain invalidate their
evidence and use [$recover](../recover/SKILL.md) skill.

For product candidates, read
[delivery schema](../iteration/references/delivery.md), emit the exact schema-v2
manifest from the actual receipt and hand it to [$batch](../batch/SKILL.md) skill.
Workers never push. Maintenance/Epoch/accepted Batch commits automatically
continue to [$publish](../publish/SKILL.md) skill unless the user chose local-only.
Output: full base/tip, actual checks, blockers and material knowledge candidates.
