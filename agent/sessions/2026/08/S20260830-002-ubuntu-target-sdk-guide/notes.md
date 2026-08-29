# Notes

## Confirmed state

- `ubuntu-20.04-target-sdk` contains the full glibc 2.31/GCC 10 target layout.
- `generic-llvm-toolchain` contains target-built LLVM/MLIR static libraries and
  LLD with matching SDK identity.
- The current `release` preset selects Release and LTO only. It remains a host
  build and does not establish the D0009 floor.
- `packaging/build.py --target-sdk` installs and validates a pre-existing build;
  it does not recompile host-built inputs.

## Probe details

The working disposable target tuple used raw Clang 22.1.8, explicit target and
sysroot variables, the SDK GCC layout and static zlib, generic LLVM/MLIR package
roots, target LLD, static C++/LLVM closure, system loader, RPATH suppression,
installed runtime LLD path, and file-prefix remapping.

A build-tree-only compiler test embedded the materialized LLD path through its
intentional `METAFLUX_TEST_LLD_PATH` definition. It is not installed package
payload. The installed `metafluxd` probe instead embedded
`/usr/libexec/metaflux/ld.lld` and passed the no-store-path check.

## Known activation gaps

- The product target tuple is not represented by a checked-in CMake entry point.
- The signed provenance verifier has no checked-in argument-resolving driver.
- The SDK fetch derivation currently names canonical Ubuntu snapshot URLs; the
  configured-timezone mirror policy remains an acquisition-route requirement,
  not a change to package identity.
