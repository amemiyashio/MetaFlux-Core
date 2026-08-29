# Backend Plugin ABI v1

Home of the sized `mf_backend_api_v1` in-process C function table. It covers
device enumeration/capabilities, compilation and loading, context/queue/memory
lifecycle, launch/copy/event/synchronization/cancellation, metrics, and policy.
The backend ABI version is independent of client-protocol versions.

All backend objects are generation-owned opaque 64-bit handles. Callers own every
input buffer and every output buffer they supply, including the two-call compile
artifact buffer; the backend never frees caller memory. The backend owns object
storage behind handles and releases it only through the paired destroy/free call.
The returned API table and extension chain have static lifetime. Input pointers
remain valid only for the duration of a call unless that call's contract creates
an opaque object from their contents.

Function-table growth is append-only and guarded by `struct_size`; capability bits
gate optional groups. Consumers ignore a larger tail and reject a table shorter
than the last function they require. No exception, STL object, compiler class,
ecosystem handle, language boolean, or allocator ownership crosses this C ABI.
v1 remains pre-stable until the CPU vertical slice and ABI qualification pass.
