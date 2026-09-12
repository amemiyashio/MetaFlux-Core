# Stock Application Entry and Service Access

Locate the current process owner before designing an entry:

| Boundary | Source |
| --- | --- |
| Development stock-client orchestration | [Baseline runner](../../../../tests/compatibility/run_pytorch_cuda_stock_baseline.py), [frontier runner](../../../../tests/compatibility/run_pytorch_cuda_cpu_frontier.py) |
| Client socket selection and connection | [fastpath.c](../../../../runtime/client/fastpath/src/fastpath.c): `METAFLUX_SOCKET`, `mf_client_session_connect_capabilities_v1` |
| Listener activation and peer admission | [server.cpp](../../../../services/metafluxd/src/server.cpp): `LISTEN_PID`, `LISTEN_FDS`, `SO_PEERCRED` |
| Installed socket/service policy | [socket unit](../../../../packaging/common/metafluxd.socket), [service unit](../../../../packaging/common/metafluxd.service) |
| Release-only inherited-listener fixture | [activation launcher](../../../../tests/release/metaflux_activation_launcher.c) |

The baseline runner uses a process-local provider search path. The frontier also
preloads its selected cuBLAS adapter. Preserve exact pinned client inputs from
`toolchains/`; upstream research references are not runtime dependencies.
Do not copy test-runner orchestration and call it a released activation entry.

## Implement the missing launch boundary

For the assigned user entry, define provider paths, daemon ownership and socket
choice before process startup. Carry the original arguments, working directory,
signals and exit status. Scope loader variables to the child and avoid altering
the parent environment or global loader configuration. Define who starts/stops
a private daemon and who only connects to a service-managed daemon; an ordinary
client exit must not kill a shared service.

The daemon accepts one matching-PID systemd descriptor at fd 3. Test incorrect
PID/count/type and ensure descriptors do not leak into unrelated children.
Current standalone socket policy and packaged socket policy differ; verify the
real credentials and access rather than relaxing modes to make a probe pass.
Keep Unix socket paths within `sockaddr_un.sun_path`; the current runners use
short `/tmp`-anchored scratch paths to avoid nested Nix TMPDIR length failures.

For AMD Vulkan, check the service process's render-node visibility and access.
The current unit has `PrivateDevices=yes`; a shell-visible render node does not
prove service visibility. Implement any required service policy within the
assigned packaging/privilege scope, and bind evidence to the actual physical
device/driver. Merely choosing a Vulkan build preset does not select the daemon
route.

The [Vulkan work item](../../../plan/milestone-0.2.0.0-pytorch-cuda-compatibility/work/work-item-0.2.0.3-framework-qualification.md)
requires persistent ordinary-process corpus activation. Release installation,
upgrade and removal belong to the
[stable release work item](../../../plan/milestone-1.0.0.0-stable-qualification/work/work-item-1.0.0.3-stable-release.md).
The [vroot launcher](../../../../packaging/vroot-launcher/README.md) emits
presentation plans and performs no compute launch. Preserve these ownership
boundaries when choosing the first implementation file.

Verify a real affected client operation after connection, clean failure when its
selected service/backend is unavailable, repeated process startup/exit and
environment restoration. Distinguish a tests-owned fd handoff from installed
stock-client activation and actual CPU/GPU completion.
