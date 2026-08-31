# NixOS Integration

`module.nix` provides `services.metaflux`: a native NixOS system user, hardened
socket-activated daemon, configurable package/socket identity, and explicit
client group membership. It does not install NVIDIA-named providers globally or
create vendor device nodes. Exact-kernel modules, vfio-user policy, and native
NixOS VM qualification remain owned by later milestones; the package-owned udev
rule is documented in `packaging/common/README.md`. Product package and module
definitions remain under `packaging/`; the repository-level `nix/` tree only
exposes fixed tools.
Callers enabling the module provide `services.metaflux.package` explicitly from
their packaging-owned product package; the repository tool flake has no daemon
package default.
