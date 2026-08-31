# Notes

The verifier is intentionally fed by a reflection record rather than parsing
SPIR-V itself. This keeps target/profile consistency and packed-argument layout
machine-checked before shader-module creation while leaving dialect conversion,
binary validation, and semantic lowering to the next compiler-owned stage.
