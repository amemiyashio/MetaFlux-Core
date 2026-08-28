# Transports

Transports connect providers, services, kernel components, guests, and execution
backends without owning compute semantics. Planned implementations include local
memfd, character-device, and vfio-user paths.

Control-plane negotiation and lifecycle are separated from steady-state command,
completion, and data movement. All transports carry the same generation-bound
handles and fixed descriptor contracts; transport choice must not leak CUDA or
backend-specific types into the core.

`metafluxd` authorizes the generation and exclusive worker lease. The leased
worker directly consumes queues: a local worker serves `memfd/` or `cdev/`, while
`metaflux-vfio-userd` serves `vfio-user/`. No control RPC is inserted into an
active queue.

MetaFlux protocol records are little-endian. A transport whose framing is
host-native, including vfio-user, adapts framing at its boundary and then uses
explicit MetaFlux encoders/decoders. Native transport structs are never cast to
device descriptors or shared records.

## Halves

Every transport owns exactly one directory and exactly two load images, never a
shared one (D0010):

- `client/`: the provider-side half. C17, statically linked into the
  application-side closure, registered with the component role
  `transport-client`, and therefore subject to the C-only directory rule.
- `worker/`: the leased-worker half. C++20, linked into the daemon or worker
  process, registered with the component role `transport-worker`.

The two halves never include each other's headers. They communicate only
through the encoded contracts (`contracts/protocol/`, `contracts/shared/`) and,
for cdev, the kernel UAPI in `contracts/uapi/linux/`. This single-directory
two-image split mirrors how Wine keeps a DLL's PE side and its `unix/` side
together while compiling them into separate load images, and it lets
`nix/lib/source.nix` grant the provider package only the `client/` halves.
Component roles are enforced by `tools/check-component-graph.py`; the transport
role rows freeze the rules the first implementation must satisfy.
