# Kernel Tests

Kernel qualification support lives here, including KUnit, kselftest fixtures,
fault injection, compat-layout checks, and lifecycle stress coverage for each
supported kernel configuration.

The cdev live qualification binary is built as
`metaflux_transport_cdev_live_qualification` when the cdev transport is enabled.
It exercises the generated UAPI against `/dev/metaflux0` and
`/dev/metafluxctl`: queue mapping, eventfd-backed worker lease, payload query and
mapping, non-aligned long-term registered memory, malformed and stale requests,
unknown ioctl rejection, and owner-close VMA tombstones. The device paths may be
overridden with `METAFLUX_CDEV_PATH` and `METAFLUX_CDEV_CONTROL_PATH`.

Run it with:

```sh
nix develop . --command ctest --preset dev \
  -R '^metaflux\\.transport\\.cdev-live-qualification$' \
  --output-on-failure
```

Return code `77` means that the target device is absent or the worker lease is
busy; it is recorded as a skipped live qualification, never as a passing test.
The binary does not load or unload the kernel module and does not fabricate
device nodes.

When host privilege is required, D0032 keeps build and test semantics here but
routes elevation through fixed actions:

```sh
nix develop . --command python3 \
  agent/skills/start-work/scripts/host_privilege.py driver load \
  /absolute/repository/build/path/metaflux_core.ko
nix develop . --command python3 \
  agent/skills/start-work/scripts/host_privilege.py driver live \
  /absolute/repository/build/path/metaflux_transport_cdev_live_qualification
nix develop . --command python3 \
  agent/skills/start-work/scripts/host_privilege.py driver logs
```

The root helper validates canonical artifact names and the configured repository
root. It does not accept arbitrary commands, module parameters, or credentials.
