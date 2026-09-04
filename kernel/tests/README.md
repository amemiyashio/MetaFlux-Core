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

## Kernel Debug CONFIG Probe

The CTest gate `metaflux.kernel.debug-qualification` runs
`tools/probe-debug-kernel.py --require-qualification` to verify that the host
kernel has the debug symbols needed for KUnit, KASAN, KCSAN, lockdep, and
kmemleak qualification. On the current 6.18 LTS host those CONFIGs are unset;
the test exits with return code 77 and is recorded as **Skipped**, never as
passing.

The companion test `metaflux.kernel.debug-config-probe` records the current
CONFIG values to `${CMAKE_BINARY_DIR}/metaflux-kernel-debug-config.json` and
always passes. The self-test `metaflux.kernel.debug-config-probe-selftest`
verifies the probe logic against synthetic fixtures without requiring a real
debug kernel.

Live KUnit/sanitizer soak remains a batch-0002 host gate; this probe only
records CONFIG presence and skips qualification when unset.

## Batch-0002 Debug Kernel Supply Path

The pinned `linux_6_12` source (6.12.105) builds an x86_64 qualification
kernel (KUnit, KASAN, KCSAN, kmemleak, lockdep, modules) via:

```sh
nix develop .#linux-debug --command tools/build-debug-kernel.sh
```

The cache lives at `../.metaflux-build/MetaFlux-Core/debug-kernel` (`src`
symlink overlay, `build` O= tree, `build/arch/x86_64/boot/bzImage`). The final
`build/.config` is authoritative: the build script reports any qualification
CONFIG dropped by Kconfig dependencies (KASAN and KCSAN may be mutually
exclusive on 6.12).

`tools/run-debug-kernel-qualification.py` is the guest supply path: it builds
`metaflux_core.ko` and `mf_cdev_generation_kunit.ko` against that kernel
(`make -C <overlay> O=<build> M=<repo>/kernel/... ARCH=x86_64 modules`),
cross-builds a statically linked `metaflux_transport_cdev_live_qualification`
into a dedicated CMake tree, packs a busybox initramfs, boots it under
`qemu-system-x86_64 -kernel/-initrd` with `console=ttyS0 oops=panic panic=-1
kasan_multi_shot`, and parses the serial console for the guest markers, the
`mf_cdev_generation` KUnit TAP block, the kmemleak scan, and post-module-load
KASAN/KCSAN/lockdep/RCU warnings. It emits a JSON summary (`kernel_config`,
`phases`, `kmemleak_empty`, `kunit`, `warnings`) and exits 0 pass / 1 fail /
77 skip. The runner never claims a sanitizer gate closed; it only supplies the
boot and run evidence.

Phases (`--skip-kernel`, `--skip-module`, `--skip-qualification-binary`,
`--skip-guest`) are independently skippable so each Nix shell can do the part
its toolchain owns:

```sh
# Kernel + module + KUnit module (gcc, gnumake):
nix develop .#linux-debug --command \
  python3 -B tools/run-debug-kernel-qualification.py --skip-qualification-binary --skip-guest

# Static qualification binary (cmake, ninja, clang):
nix develop . --command \
  python3 -B tools/run-debug-kernel-qualification.py --skip-kernel --skip-module --skip-guest

# Guest boot and console parse (qemu + static busybox):
nix develop .#vfio-user --command \
  env STATIC_BUSYBOX="$(command -v busybox)" \
  python3 -B tools/run-debug-kernel-qualification.py \
  --skip-kernel --skip-module --skip-qualification-binary
```

`STATIC_BUSYBOX` (or `--static-busybox`) must point at a static busybox; the
vfio-user shell packages one via `pkgs.busybox.override {enableStatic=true}`.
An unset value exits 77, as do a missing bzImage (run
`tools/build-debug-kernel.sh`), missing `qemu-system-x86_64`, and missing
`cmake`/`ninja`. The runner checks each `.ko` vermagic against the kernel
release before shipping it to the guest. The CTest row
`metaflux.kernel.debug-kernel-selftest` runs only the parser/config self-test
(`tools/test-run-debug-kernel-qualification.py`); booting the real guest stays
a manual batch gate.

## KUnit Execution

The CTest gate `metaflux.kernel.kunit-generation` runs
`tests/kernel/probe-kunit-generation.py`, which delegates to
`tools/run-kunit-generation.py` to build and execute the
`mf_cdev_generation` out-of-tree KUnit suite using the pinned
`linux_6_12` source (6.12.105). The runner is launched from the
`linux-debug` Nix shell (`nix develop .#linux-debug`), which sets
`METAFLUX_LINUX_SRC` to the materialized source root. On hosts without
`METAFLUX_LINUX_SRC` the gate exits 77 and CTest records the test as
**Skipped**.

The probe is NOT gated on host `CONFIG_KUNIT`; that belongs only to the
`metaflux.kernel.debug-qualification` probe.

## Host Privilege

When host privilege is required, `manage-host-privilege` applies decision-0032 while
build and test semantics remain here:

```sh
nix develop . --command python3 \
  agent/skills/manage-host-privilege/scripts/host_privilege.py driver load \
  /absolute/repository/build/path/metaflux_core.ko
nix develop . --command python3 \
  agent/skills/manage-host-privilege/scripts/host_privilege.py driver live \
  /absolute/repository/build/path/metaflux_transport_cdev_live_qualification
nix develop . --command python3 \
  agent/skills/manage-host-privilege/scripts/host_privilege.py driver logs
```

The root helper validates canonical artifact names and the configured repository
root. It does not accept arbitrary commands, module parameters, or credentials.
