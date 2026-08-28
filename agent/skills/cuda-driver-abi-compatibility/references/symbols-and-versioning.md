# Symbols and Versioning

Load this reference for export generation, version aliases, typed stubs, or
`cuGetProcAddress` work.

## Repository checklist

1. Name the selected header packages and hashes. The open M0001 header
   acquisition decision must close before the W04 manifest freezes.
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
