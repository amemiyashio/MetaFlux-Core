# Notes

The new entry is intentionally a thin producer boundary: it captures the
Coordinator snapshot once, then delegates to `submit_external_event`. A stale
snapshot remains visible as a normal lifecycle result rather than rebinding to
the later generation. QMP command correlation keeps its existing pre-captured
event path because its completion may arrive after the command observation.

Convergence found that the initial generic helper also admitted delayed QMP and
other event kinds, while its regression omitted the promised malformed and
stale-observation cases. The final helper accepts only admin reset, VFIO-user
reset, disconnect, and daemon restart. Known events owned by another producer
path and unknown event kinds both return `Unsupported` without authority
mutation. The helper is one producer call, not a promise that capture and apply
are indivisible against concurrent authority requests.
