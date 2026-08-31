# Notes

The generic-release preset requires explicit SDK environment variables; the
repository-owned `tools/build-generic-release.sh` resolved the locked SDK and
generic LLVM paths before configuring the build. The package policy remains
portable and does not change kernel UAPI or device-node creation.
