# Exact Recovery

Run commands through the clean Nix entry. Read the current request and inspect
its recovery token before selecting an action.

`resume [--revision FULL_COMMIT]` rechecks exact context. A validated committed
workflow record restores publication/handoff of that same commit. Unrelated
HEAD changes require a new application base. Before commit, lost state or stale
content requires fresh scope review and verification; do not infer acceptance.

`rescope --expected-state TOKEN --paths-json JSON --reason TEXT` adds necessary
same-task paths while preserving previous paths, objective, assignment, base,
checks and publication. It grants no Goal ownership. Success returns to
preparation; load rules, prepare, implement, review and verify the revised scope.

`supersede --expected-state TOKEN --request-json JSON --reason TEXT` replaces
only an uncommitted operation with an explicitly confirmed Epoch request.
First preserve unrelated edits in Git outside the governance tree; retain their
exact object, modes/blobs and staged disposition for restoration after publication.
Older-Epoch product work needs new matching context/evidence before acceptance.

Both commands hold the common Git lock, validate the exact token twice, reject
pending acceptance/committed replacements, and invalidate old request bindings.
They do not mutate product files or create contexts. Never edit temporary state
to simulate recovery. Interrupted acceptance restores only transaction-owned
before/after files and index entries; unrelated changes remain visible.

`step repair` returns pre-commit work to implementation and invalidates review
and receipt. Optional checks must retain required coverage under the verification
contract. An unchanged staging repair instead stays in delivery with its receipt.
