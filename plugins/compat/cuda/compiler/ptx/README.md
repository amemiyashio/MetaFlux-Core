# CUDA PTX Frontend

This directory owns the CUDA PTX input adapter into the ecosystem-neutral
compiler core. The current target is a build and contract fixture exercised by
smoke tests; it is deliberately absent from installed daemon artifacts until the
compiler worker owns frontend registration and execution.

The frontend may depend on compiler contracts, but it must not depend on an
execution backend or application-side CUDA provider DSO.
