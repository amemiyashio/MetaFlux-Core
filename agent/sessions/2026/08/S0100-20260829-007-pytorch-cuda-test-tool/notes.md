# Notes

The wheel locks are complete runtime resolutions, not pip command transcripts.
Each contains 29 unique wheels and preserves the canonical filename, byte size,
and SHA-256. In Asia/Shanghai, Aliyun is first; the final second route uses
Kakao for available PyPI files, a Tokyo CloudFront edge for the two PyTorch
wheels, and USTC for four newer PyPI entries not yet present on Kakao. Every
second candidate returned HTTP 200 with the declared byte size; canonical
upstream remains last and the frozen hash remains authoritative.

Both closures were fully built in task-owned local Nix stores and imported
before the final transfer-route refresh. The refresh changed only candidate
URLs. Nix fixed-output hashes, the static manifest gate, final route size checks,
and post-refresh dry-runs preserve the same binary inputs without repeating a
multi-gigabyte materialization. The local stores peaked at roughly 12 GiB for
baseline and 9.2 GiB for frontier and were removed by their scoped traps. The
host store remained about 12 GiB.

PyTorch import emits an optional NumPy interoperability warning because NumPy
is not in either frozen Requires-Dist closure. Import, architecture inspection,
and CPU tensor addition pass; the compatibility probe does not use NumPy. A
future NumPy-dependent test must declare that client input deliberately.

The probe is diagnostic by default and exits successfully after reporting the
first gap. `--require-stage` turns only the named stage into a gate. It removes
`PYTORCH_NVML_BASED_CUDA_CHECK` before import and direct driver enumeration,
then restores the caller's value in `finally`, so NVML cannot substitute for the
CUDA driver path. Private PyTorch entries are intentionally version-bound by
the two exact profiles; entry drift is reported as a probe gap.
