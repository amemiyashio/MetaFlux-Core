# Task-Stop Diagnostics

`tools/agent_diagnostics.py` owns the single diagnostic shape:
`code`, `source`, `summary`, `evidence[]`, `responsibility`, `disposition`,
`required_action`, `resume_when`, and optional exact `retry_command`.

Responsibility: current-agent, user-or-application, batch-integrator,
epoch-governor or host-operator. Disposition: fix-and-retry, stop-and-report or
preserve-and-report. Preserve child diagnostics and raw product-tool output.
Retry a current-agent repair only after its cause or prerequisite changes.
An omitted Agent-declared path is not withheld user permission.

A failed delivery blocks that phase; continue independent in-scope work when it
still yields useful evidence. External responsibility does not authorize
messages, scheduling, privilege, cleanup or an unchanged retry. Name the exact
failed phase and remaining useful action. Success claims must match the tested
behavior and inputs. Diagnostics are output/current recovery data, not a ledger.
