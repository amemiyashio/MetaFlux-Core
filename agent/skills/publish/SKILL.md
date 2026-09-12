---
name: publish
description: Publish one exact committed MetaFlux delivery through its canonical governed Git transport, verify remote ancestry, or diagnose that transport read-only.
---

# Publish

Role: controlling parent with one full committed revision.
First action: reload publication rules at the committed HEAD and read
[transport](references/transport.md). A pre-commit certificate is stale here.

Run `main.py step publish` through the clean Nix entry for the controller's
exact commit. Maintenance, Epoch activation and Batch acceptance publish
automatically unless the user limits publication. Ordinary worker candidates
go to [$batch](../batch/SKILL.md) skill instead.

For explicitly requested standalone publication or transport repair, use
`scripts/push_repository.py` with the exact full revision. Diagnostic check and
dry-run modes remain read-only. Never read private-key bytes or infer a revision
from an unstaged tree. Keep the pinned destination and governed transport.

Completion: report the full published and observed remote main revisions, with
equality or actual ancestry proof. Failure preserves the same commit and uses
[$recover](../recover/SKILL.md) skill; it does not repeat acceptance or commit.
Follow the handoff action card after successful publication.
