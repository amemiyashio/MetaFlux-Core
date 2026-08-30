# Notes

This is a one-time breaking identity migration under D0024. M/W/S identifiers
are derived from explicit four-part delivery coordinates, so every historical
record is renamed and rewritten instead of retaining a legacy alias. Product
SemVer remains three-part; ABI, protocol, schema, SONAME, and third-party
versions remain separate namespaces.

Compact bodies are intentionally not parsed back into components. The dotted
coordinate is authoritative, and repository/session creation gates reject a
different dotted coordinate that would produce an existing compact body.
