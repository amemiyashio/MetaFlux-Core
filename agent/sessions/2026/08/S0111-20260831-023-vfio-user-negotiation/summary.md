# Session Summary

## Objective and outcome

Implement the W0111 transport ABI 0.x negotiation path on the existing vfio-user guest/server fixture, with generated schema constants and focused regression coverage.

The guest now encodes a zeroed negotiation request with candidate version and
feature bits, while the server validates it, selects a compatible minor and
feature set, and publishes registry identity, generation, queue, DMA, and
in-flight limits. Unsupported versions return the common completion status;
successful replies retain the canonical negotiation record.

## Durable changes

- `contracts/protocol/transport/v1/schema/vfio_user.json`: added the
  `MF_VFIO_USER_MESSAGE_NEGOTIATE_V0` message constant and refreshed manifest
  hashes, including the lifecycle extension import.
- `transports/vfio-user/guest/`: added negotiation encode/decode helpers with
  request/response shape validation.
- `transports/vfio-user/server/`: added configurable transport capabilities and
  server-side negotiation selection/rejection.
- `contracts/protocol/transport/v1/README.md`: documented the 0.x exchange.

## Verification

| Command/gate | Result |
| --- | --- |
| `python3 tools/validate-transport-schema.py --root .` | Passed: 5 definitions / 15 records |
| Focused CTest | Passed: schema, guest/server, component graph, lifecycle model 8/8 |
| Full development CTest | Passed: 84/84 |
| `git diff --check` and CMake build | Passed |
| Content identity | `fa515bb`, Agent Harness (codex) as Author and Committer |
| Agent records before checkpoint record | Passed before this session record update |

## Cleanup

- Removed: no session-owned disposable artifacts.
- Retained: the generated-schema negotiation path, focused regression, and
  compact checkpoint evidence.

## Decisions and experience

- W0111 remains Active because live QEMU/libvfio-user negotiation and local cdev
  activation are outside this host-independent slice.

## roast

### light roasts

- Guest/server capability selection -> `transports/vfio-user/` (`fa515bb`; focused
  and full CTest)

### medium roasts

- W0111 transport negotiation boundary ->
  `agent/plan/M0110-kernel-guest-transport/work/W0111-abi-benchmark-contract.md`
  (P068; live activation and measurement evidence remain open)

### dark roasts

- none.

## session-only

- Memfd/socketpair fixture - reason: proves codec and server selection without
  claiming physical QEMU, PCI, or NVIDIA qualification.

## Unresolved items

- W0111: connect the negotiated limits and identity to pinned local cdev and
  live QEMU/libvfio-user activation; retain the 0.x schema as authority.

## Handoff

Resume from `fa515bb` and
`agent/progress/checkpoints/2026/P20260831-068-m0110-vfio-user-negotiation.md`.
Read W0111, then continue with the local/guest activation gate.
