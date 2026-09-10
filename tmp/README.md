# Workspace Scratch

This directory is the only in-repository home for workspace scratch
(decision-0042). Git tracks this README so the directory exists; every other
path under `tmp/` is host state and stays gitignored.

| Kind | Home |
| --- | --- |
| CMake/Ninja trees | `tmp/build/<preset>` |
| Debug-kernel overlay and `O=` tree | `tmp/build/debug-kernel` |
| Measurement dumps, package-matrix evidence, lifecycle checker JSON | `tmp/outputs/<name>` |
| Explicit retained work directories | `tmp/work/<name>` |

CMake presets select `tmp/build/<preset>`. Custom configurations use a distinct
name below `tmp/build/`; the optimization runner allocates and cleans a fresh
`tmp/build/optimization-<unique>` tree unless `--keep-work` is requested.
Build trees are not portable: their caches contain absolute paths, so discard
obsolete configurations and configure again rather than moving a cache into a
different directory. Logs and probes needed to reproduce an unresolved result
remain with their owning work or output directory.

[`MetaFluxBuildDirectory.cmake`](../cmake/MetaFluxBuildDirectory.cmake) validates
workspace build locations before compiler detection. Build scripts invoke the
same check before materialization or directory writes. It resolves symlinks and
`..`; repository-external build directories and disposable test fixtures remain
valid. The guard itself does not create or clean directories.

The invoking CMake, Ninja, CTest, or harness owns creation and cleanup. Nix
does not. Installed daemon/AOT roots remain `/var/cache/metaflux/compiler` and
`/var/lib/metaflux/aot` (decision-0014) and are not relocated here. Guest or
container `/tmp` inside a qualification image is that image's filesystem, not
this directory.

Do not revive `build/`, `.cache/`, `outputs/`, `.metaflux-build`,
`.metaflux-evidence`, `../.metaflux-build`, or `../.metaflux-evidence` as
scratch homes. The root [`.gitignore`](../.gitignore) ignores `tmp/**`
except this README, and keeps those obsolete names ignored so leftover
host trees cannot re-enter Git. Kernel Kbuild outputs stay ignored by
[`kernel/.gitignore`](../kernel/.gitignore); `kernel/core/` is source,
not scratch.
