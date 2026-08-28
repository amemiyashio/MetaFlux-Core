# Session Outputs

Numbered files preserve MetaFlux project command output that is too large for an
inline event or verification evidence worth retaining with an exact digest.
This directory does not copy generated build trees or Nix store paths.

| Output | Description |
| --- | --- |
| [0001.txt](0001.txt) | Engineering-bootstrap verification matrix |

The retained matrix includes full development and ASan test suites, focused
provider configurations, the flake gate, and all four Nix packages. Reproducible
build outputs remain defined by repository sources, the flake lock, and CMake
configuration. See the [session summary](../summary.md).
