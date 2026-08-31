# Notes

The fixture mutation is deliberately limited to test-owned shared mappings:
the header and view-control revision change together to model a later mapping
publication. Provider state does not reread this field during an active
initialization epoch. The attach check catches a torn or stale revision pair
before any provider can enumerate it.
