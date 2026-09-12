# Provider Selection and Vendor Namespace Lifetime

Start at [passthrough.c](../../../../plugins/compat/cuda/passthrough/src/passthrough.c)
and the [component contract](../../../../plugins/compat/cuda/passthrough/README.md).
Search `mf_cuda_runtime_mode_parse_v1`, `mf_pt_load_namespace`, and
`mf_cuda_passthrough_pair_release_v1`, then inspect the CUDA/NVML provider callers
of the affected transaction. Change selection or lifetime at this mechanism;
leave exported API behavior with its provider expert.

`managed`, `passthrough` and `auto` are distinct modes. Auto attempts managed
initialization and transfers to the vendor pair only after rollback succeeds
and the managed state is pristine. A partial managed state is an error, not an
invitation to try another provider. No constructor should choose the mode.

Vendor discovery uses the explicit paired config or the component's bounded
distribution locations. Preserve root ownership, ancestor permissions, regular
file checks, MetaFlux recursion rejection, architecture/SONAME/Build-ID and
driver-build matching. Do not substitute ambient `LD_LIBRARY_PATH`, a bare
`dlopen` name or shell discovery for the vendor-pair policy. Process-local
activation of MetaFlux itself and discovery of a trusted vendor pair have
different inputs.

The CUDA library opens a new local glibc link-map namespace; NVML joins the same
namespace. Publish the pair only after versioned bootstrap lookup succeeds.
Release NVML before CUDA and release the pair once. Fork, PID/mount namespace,
configuration, driver or library-fingerprint change invalidates the pair;
stale handles do not acquire a new identity by retrying their lookup.

Implement the affected transaction and its failure cleanup together. Existing
[component tests](../../../../plugins/compat/cuda/passthrough/tests/passthrough_test.c)
cover strict discovery, fixture DSOs, namespace loading and replacement. The
real-provider CUDA/NVML mode integration gate covers public entry behavior.
Fixture vendor libraries are not physical NVIDIA coexistence evidence; physical
NVIDIA and dual-driver promotion retain their declared later qualification scope.
Do not introduce a new system-wide loader mode or privilege action during a
read-only diagnosis.
