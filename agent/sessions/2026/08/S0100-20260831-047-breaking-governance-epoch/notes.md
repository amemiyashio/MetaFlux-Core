# Notes

SC0006 correctly serialized content through one focus owner, but it still
allowed an old `in_progress` ledger to receive a later focus handoff. SC0007
removes that implicit compatibility route: executable authority requires a newly
scaffolded session whose `governance_epoch` matches the current focus. Every
schema version 1 detailed ledger is liquidated from the current tree after the
committed inventory and roast-owner audit. Old bytes remain only in Git history;
a compact ID tombstone is administrative resolution, not task context.

The transition state is bounded by Active SC0007. It is migration state, not a
compatibility mode: only this epoch-bearing governance owner may change durable
content, and the migration must end by retaining only existing medium/dark
canonical owners, deleting legacy details, and installing an epoch-bearing
product successor.
