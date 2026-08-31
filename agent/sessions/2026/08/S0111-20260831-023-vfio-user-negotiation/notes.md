# Notes

The negotiation record is intentionally reused as the successful response so
the candidate ABI does not gain a second result shape. Rejections use the
existing completion status record. The server remains compatible with the
pre-existing GET_INFO-first fixture, while new tests prove that the explicit
negotiation path can reject an unsupported minor and then accept the current
minor with optional-feature intersection.
