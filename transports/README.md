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
