# Notes

The Nix deletion set was the union of dead MetaFlux source-flake paths and
explicit old product/check/package name seeds, expanded only to their dead
referrer closure. Before deletion it contained 1,294 paths, intersected neither
the live set nor fixed tool names, and had no external referrer. This exact-set
method avoided broad GC and preserved the 104-path tool cache.
