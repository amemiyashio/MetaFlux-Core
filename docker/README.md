# Container Build Contexts

This directory is the approved home for container build contexts and
Dockerfiles owned by repository release qualification. The approved container
runtime is podman, exposed by the release development shell:

```sh
nix develop .#release
```

Release rows keep the established transport policy: digest-pinned image
references, `--pull=never`, and `--network=none`. See
[`tests/release/README.md`](../tests/release/README.md) for the row contracts;
base-image acquisition remains a separately recorded operator step.
