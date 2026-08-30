# Architecture Decisions

This directory records architectural descriptions and decision records. Plans
under `agent/plan/` describe intended milestones. Architecture documents carry
an explicit status: `Proposed` mirrors a plan boundary, while `Verified` means the
decision has been implemented and tested. Only `Verified` documents define a
stable repository contract.

Documents should state ownership, dependency direction, public contracts,
performance consequences, and compatibility impact. Future compatibility
ecosystems or general plugin APIs are documented here before their interfaces are
frozen, without creating speculative source directories.

The current proposed boundaries are summarized in
`control-and-data-plane.md`; detailed qualification remains in M0110 and M0120.
The directory taxonomy and dependency-direction map are recorded in
[`repo-layout.md`](repo-layout.md) (Verified). Decision-authorized breaking
repository migrations follow the verified
[`semantic-change-governance.md`](semantic-change-governance.md) contract
(D0025).
