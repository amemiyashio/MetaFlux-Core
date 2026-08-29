# Notes

The glibc boundary is enforced in the owning workflows. The target SDK and
generic LLVM manifests are checked before packaging, and their recorded SDK
build/package identities must agree. Package staging inspects every ELF for the
system loader, dynamic dependency closure, RPATH/RUNPATH, Nix store strings,
and the `GLIBC_2.31` ceiling. The release harness applies the same checks to
the activation launcher and CUDA acceptance fixture before any container starts.

The release matrix intentionally still exercises Ubuntu 22.04, Ubuntu 24.04,
and Rocky 9. Those are runtime qualification rows for one generic x86_64
artifact, not additional SDK or glibc targets. Temporary package, evidence,
and build paths were removed after their results were summarized in the
checkpoint and session record.
