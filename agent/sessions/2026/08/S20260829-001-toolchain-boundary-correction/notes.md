# Notes

The failed CMake compiler probe was caused by wrapping the Nixpkgs Clang wrapper
a second time with `makeWrapper --prefix NIX_LDFLAGS`. Token de-duplication
removed an existing `-rpath` token but left its `$out/lib` argument, so LLD
treated the repository-local `outputs/out/lib` path as an input file. Removing
the second wrapper and setting `NIX_NO_SELF_RPATH=1` in the development shell
restored C/C++ linking without transferring RPATH policy to CMake.
