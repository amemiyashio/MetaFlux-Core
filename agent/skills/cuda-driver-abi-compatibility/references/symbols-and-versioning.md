# Symbols and Versioning

Load this reference for export generation, version aliases, typed stubs, or
`cuGetProcAddress` work.

## Repository checklist

1. Read the selected packages and hashes from the current
   [decision-0016 inputs](../../../../toolchains/README.md#cudanvml-abi-inputs-decision-0016)
   and [header manifest](../../../../toolchains/nvidia-headers-1.json).
   Reuse that frozen identity; header acquisition is already decided.
2. Generate, rather than manually duplicate, the canonical symbol, required
   `_v2`/`_v3`, PTDS alias, declaration, version node, and implementation status.
3. Keep implementation symbols hidden. Export only the manifest surface through
   the linker version script and prove no accidental C runtime or project symbol
   escapes.
4. Give known but unsupported entries correctly typed stubs. Return the CUDA
   error assigned by the compatibility contract; never return a null function
   pointer as an undocumented substitute.
5. Model `cuGetProcAddress` as an ABI resolver with symbol name, requested CUDA
   version, flags, returned status, and selected alias. Test lower, exact, higher,
   malformed, and unsupported requests.
6. Separate driver API version reporting from package/build versioning. Record
   both in qualification output.
7. Keep DSO load inert: no project constructor, thread, socket, registry attach,
   or heap state before the first real API call.

## Evidence

Start with the [symbol authority](../../../../plugins/compat/cuda/abi/driver/symbols.def),
[provider dispatch](../../../../plugins/compat/cuda/abi/driver/src/dispatch.c),
[surface profile](../../../../plugins/compat/cuda/abi/driver/profiles/pytorch-cuda-cpu-v1.json)
and its [validator](../../../../plugins/compat/cuda/abi/driver/tests/validate_surface_profile.py).
Check the affected declaration, API version and alias before changing behavior.
When an ABI surface changes, prove calling convention and data layout as well as
symbol spelling; a matching export name alone is insufficient.

- Diff canonicalized exports, ELF symbol versions, sizes, bindings, and
  visibility against a checked-in expected manifest.
- Test both direct linking and `dlopen`/`dlsym`, then co-load CUDA and NVML.
- Inspect `DT_NEEDED` and the glibc symbol ceiling under the release sysroot.

## Primary sources

- [CUDA Driver API](https://docs.nvidia.com/cuda/cuda-driver-api/)
- [Driver entry-point access](https://docs.nvidia.com/cuda/cuda-driver-api/group__CUDA__DRIVER__ENTRY__POINT.html)
- [CUDA version-mixing rules](https://docs.nvidia.com/cuda/cuda-driver-api/version-mixing-rules.html)

Exact behavior is fixed by the selected headers and tested driver families, not
by the latest online documentation alone.
