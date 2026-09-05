# Architecture Records

Architecture records define cross-component behavior that is broader than one
source owner. Their frontmatter status determines maturity; current source and
passing tests remain stronger evidence than prose.

- [`repo-layout.md`](repo-layout.md): repository ownership, language walls, and
  dependency boundaries.
- [`control-and-data-plane.md`](control-and-data-plane.md): registry authority,
  leased workers, and lifecycle ownership.
- [`agent-execution.md`](agent-execution.md): decision-0033 Epoch/Batch/Iteration
  execution and destructive governance.
- [`agent-tool-detection.md`](agent-tool-detection.md): decision-0034
  conversation-emitted harness name and dynamic commit identity.
- [`host-privilege-escalation.md`](host-privilege-escalation.md): bounded,
  non-secret host privilege and driver-debug helpers.

Product-specific behavior remains in source, tests, contracts, and approved
milestone/work-item plans rather than being duplicated here.
