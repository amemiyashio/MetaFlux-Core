# Notes

SC0006 correctly serialized content through one focus owner, but it still
allowed an old `in_progress` ledger to receive a later focus handoff. SC0007
removes that implicit compatibility route: historical and legacy-active ledgers
remain evidence, while executable authority requires a newly scaffolded session
whose `governance_epoch` matches the current focus.

The transition state is bounded by Active SC0007. It is migration state, not a
compatibility mode: only this epoch-bearing governance owner may change durable
content, and the migration must end by installing an epoch-bearing product
successor.
