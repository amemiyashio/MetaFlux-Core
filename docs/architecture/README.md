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
`control-and-data-plane.md`; detailed qualification remains in Plans 0002.1 and
0002.2.
