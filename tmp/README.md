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

The invoking CMake, Ninja, CTest, or harness owns creation and cleanup. Nix
does not. Installed daemon/AOT roots remain `/var/cache/metaflux/compiler` and
`/var/lib/metaflux/aot` (decision-0014) and are not relocated here. Guest or
container `/tmp` inside a qualification image is that image's filesystem, not
this directory.

Do not revive `build/`, `.cache/`, `outputs/`, `../.metaflux-build`, or
`../.metaflux-evidence` as scratch homes.
