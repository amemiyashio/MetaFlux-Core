# Notes

SC0006 correctly serialized content through one focus owner, but it still
allowed an old `in_progress` ledger to receive a later focus handoff. SC0007
removes that implicit compatibility route: executable authority requires a newly
scaffolded session whose `governance_epoch` matches the current focus. Every
schema version 1 detailed ledger is liquidated from the current tree after the
committed inventory and roast-owner audit. Old bytes remain only in Git history;
a compact ID tombstone is administrative resolution, not task context.

SC0007 is settled at effective revision
`97248239d507028309df07aaa4d1f462388cf4b6`. The current tree contains no
schema version 1 session details or transient guidance. The tombstone carries
administrative resolution only; the 72 medium and 24 dark mappings continue in
62 existing canonical owners and were not copied into a roast archive.

The governance owner closes in the same record commit that installs
`S0112-20260901-001-m0110-w0112-current-epoch` as the D0029 product owner. All
later work begins from current canonical repository files.
