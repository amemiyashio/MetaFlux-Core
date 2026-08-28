# Notes

Scope was strictly record-level: no product code, contract header, or build
semantic changed. The only build-file touch is a comment in
`tests/CMakeLists.txt` documenting the provider `DT_NEEDED` universe; the
expected set and gate mechanics are unchanged.

## Review findings that drove the amendments

1. `kernel/vroot/README.md` spoke of an optional NVIDIA identity while
   `agent/memory/project.md` disclaimed vendor identity and vendor-private
   kernel ABI claims. M0003 already contained half of the intent
   (`identity=nvidia`, pre-bind, never vendor-driver eligible), so the fix was
   to bridge the records through D0008 rather than to delete the capability.
   The registration/legal review requirement was added to M0003 decision 4
   because a synthetic NVIDIA VID at release is a compliance question, not an
   engineering one.
2. The M0001 provider closure gates mathematically required glibc >= 2.34
   (threads and dlopen symbols live in libpthread/libdl below 2.34) while M0001
   decision 1 still listed the glibc baseline as open. The floor is now closed
   at 2.31 / Ubuntu 20.04 per the owner's direction, which keeps RHEL 8-class
   systems out of scope but preserves the closure gates in weakened form
   (allowed universe instead of libc-only).
3. The zero-syscall active-queue gate was physically unsatisfiable on the v0.1
   memfd transport, which has no doorbell primitive: waking a sleeping worker
   requires a syscall from someone. M0002 solves this with BAR2/ioeventfd on the
   guest path, so the zero-syscall obligation was scoped to doorbell transports
   and the memfd path was given an explicit at-most-one-wake-syscall budget.
   Worker spin/sleep policy remains worker-side implementation freedom.
4. M0001 carried precise numeric budgets with no measurement evidence and no
   status mechanism, contradicting the project's contracts-on-demand
   philosophy. Budgets now carry provisional/binding status with an explicit
   promotion condition.
5. "One authoritative logical-device view" read as global enumeration while
   M0001-W06 coexistence is namespace-based isolation. The view is now scoped
   per managed domain.

## Files amended

- `agent/README.md` (budget status rule)
- `agent/memory/constraints.md` (glibc floor)
- `agent/memory/decisions-index.md` (D0008, D0009)
- `agent/memory/project.md` (D0008 bridge, per-domain view)
- `agent/plan/M0001-core-foundation/plan.md` (decision 1 closure, DT_NEEDED
  universe, memfd wake budget, budgets status)
- `agent/plan/M0003-vpci-lifecycle/plan.md` (D0008 cross-reference, legal
  review in decision 4)
- `agent/templates/milestone.md` (budgets front-matter field and promotion
  rule)
- `docs/architecture/control-and-data-plane.md` (local wake and syscall
  budget paragraph)
- `kernel/vroot/README.md` (D0008 disguise semantics)
- `tests/CMakeLists.txt` (DT_NEEDED universe comment)
- `transports/memfd/README.md` (wake contract reference)

## Open items intentionally left

- M0002 data-plane v1 freeze still precedes any device-class backend; the
  extension-namespace decision (M0002 decision 1) is the designated mitigation
  and was not altered.
- The `metafluxd` bootstrap exit code 64 on the ready path was judged a
  defensible fixture semantic and left unchanged.
- Worker spin/adaptive/sleep policy itself is deliberately not fixed by these
  amendments; it stays an implementation freedom until M0002 evidence exists.
