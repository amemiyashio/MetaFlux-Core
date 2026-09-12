# Candidate Integration

Preconditions: exact delivery passed check; Batch operation is prepared;
implementation rules are loaded. Review base..tip against the briefing and
owning Exit Gate. A current-HEAD candidate stays in place. For divergence use
`git merge --no-commit --no-ff TIP`; a stale ancestor is not a new delivery.

Resolve bounded composition defects only. Promote material knowledge before
final integration evaluation. Select fresh covering checks for the combined
tree, preserving modes, prerequisites and full Exit Gate coverage when claimed.

1. `batch.py load-rules DELIVERY --root .` emits complete integration review
   and verification modules, bound to delivery.base and current HEAD.
2. Review those bodies and the actual merged behavior; then
   `batch.py verify DELIVERY --root . --checks PLAN.json --summary TEXT`.
   The wrapper validates context and preflights before any long check.
3. Pass that exact integration receipt to `batch.py advance DELIVERY --receipt
   RECEIPT --root .`. Only it advances accepted state.
4. `batch.py check-metadata --root .` proves the post-integration delta consists
   solely of the recorded Goal/work-item transaction, with before/after modes
   and blobs. Reload normal Batch review rules at its operation base and review
   the advanced tree. Verification then executes only the fixed metadata plan.

The Batch operation base is current main; the integration receipt base is the
candidate's delivery base. Never hand-assemble another baseline or replace the
integration rule certificate with ordinary Batch rules before verify/advance.
Candidate evidence does not replace fresh integration.

The exact transaction proof avoids a third product run; it is no general
documentation exemption. Product, rule, or other semantic changes require
repair and fresh integration. Failed transactions restore only their own
remaining writes. Committed acceptance binds delivery identity and candidate
revision; duplicate acceptance validates that record before checking the now
advanced Iteration number.
