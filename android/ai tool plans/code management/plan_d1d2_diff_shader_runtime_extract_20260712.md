# Shader runtime helper extraction plan

## Goal

Move byte-identical shader/program log reading and Android-aware program binding
out of both upstream-original `oglprog.c` files.

## Boundary and validation

- Keep shader source, compile/link sequencing, errors, uniforms, and program
  ownership local.
- Move only log retrieval/forwarding and the GLES3-shim-aware bind operation.
- Compile one shared source into each prefixed OGL target when OGL merge is on.
- Build desktop and Android, then run merged-wall/two-pass graphics coverage.
