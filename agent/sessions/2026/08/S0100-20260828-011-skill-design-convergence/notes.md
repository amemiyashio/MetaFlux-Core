# Notes

The original fifteen-skill catalog had useful domain depth but three structural
problems: the ecosystem-neutral registry/shared-ABI boundary had no owner,
several neighboring skills claimed overlapping semantic or target decisions,
and routing evidence was a prose list without an executable contract.

The convergence review added `runtime-contracts-registry`, narrowed every
domain description and composition route, and synchronized the affected
milestone plans. The largest technical correction was the W0102/M0120
publication gate. Review exposed that cursor updates, admission close, telemetry
publication, range allocation, and crashed or delayed writers could not be
specified independently. The final planning contract now names the owner,
linearization point, record states, recovery proof, exhaustion behavior, and
quarantine result for each operation.

Static and behavioral evidence remain intentionally separate. Repository gates
prove package/catalog/corpus integrity and exercise their own negative cases.
The observation scorer can bind real Codex runs to exact corpus and routing-
input digests, but the full 68-case, three-repetition run still needs to be
executed and archived for a named model and host before claiming behavioral
routing qualification.
