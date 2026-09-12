---
name: prepare
description: Prepare one MetaFlux mutation from its exact baseline, scope and execution context, or answer a read-only implementation-readiness or identity request.
---

# Prepare

Role: the parent responsible for this operation. Input: user intent, current
HEAD and any application-supplied assignment. Output: a prepared bounded request.
First action: inspect Git state and the relevant authority; for a mutation read
[request schema](references/request.md), then begin that request.

Run repository commands through [$main](../main/SKILL.md) skill Bootstrap.
Identity/topology helpers are in `scripts/`; identity uses only the
conversation-emitted harness name. Read-only identity/readiness creates no
operation. A full readiness review additionally reads
[evidence framework](references/evidence-framework.md); ordinary implementation
needs only the prerequisites that affect its first edit.

For a new mutation, use these adjacent steps through the clean Nix entry:

1. `main.py begin --request-json REQUEST_JSON` at the exact current base.
2. `main.py load-rules`; read every emitted preparation module completely.
3. `main.py step prepared`; then load the returned implementation card's rules
   before the first edit.

Include necessary source, tests, manifests and canonical summaries in scope.
Keep existing edits; changes after begin and before prepared invalidate
preparation. Product candidates require the exact application assignment.
Batch intake reads [$batch](../batch/SKILL.md) skill before declaring its request;
governance reads [$epoch](../epoch/SKILL.md) skill and uses existing confirmation.
The controller validates scope/evidence, not user intent.

Completion: the state is implementation at the expected base and the matching
implementation modules have been emitted. Proceed to the requested behavior;
do not repeatedly audit established prerequisites. A necessary omitted file uses
[$recover](../recover/SKILL.md) skill's rescope, not a fabricated new assignment.
