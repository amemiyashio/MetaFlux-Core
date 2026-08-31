# Notes

The mutex protects authority state and bounded request/tombstone tables, not
transport payloads or provider fence payloads. Concurrent tests use a barrier
to race reset/remove/add requests with snapshots and generation resolution;
they must assert serialized results and final state rather than assume a
particular winner for competing request order.
