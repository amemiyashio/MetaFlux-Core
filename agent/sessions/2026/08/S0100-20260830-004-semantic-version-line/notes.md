# Notes

D0024 is the first completed pre-framework identity migration. Later semantic
replacements follow D0025 and an Active SC. M/W/S identifiers are derived from
explicit four-part delivery coordinates, so every historical record in that
migration was renamed and rewritten instead of retaining a legacy alias.
Product SemVer remains three-part; ABI, protocol, schema, SONAME, and
third-party versions remain separate namespaces.

Compact bodies are intentionally not parsed back into components. The dotted
coordinate is authoritative, and repository/session creation gates reject a
different dotted coordinate that would produce an existing compact body.
