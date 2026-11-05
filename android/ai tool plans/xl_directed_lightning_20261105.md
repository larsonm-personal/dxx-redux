# Directed lightning qualification

Base: cmake e145e553a3c9c596c6f8f8f4ca46a520d23f95c4

1. Retain the reference endpoint and implement pinned-source directed random sampling under the existing style-0/no-children profile
2. Extend extracted-source geometry/emitter comparisons and native ownership, rendering, lighting, audio and save controls; preserve rejection and classic isolation
3. Verify unchanged Sphere sp-02.rl2 (SHA-256 a821cc678a6e9f574fe5ec96b9475533e687aa338676bd5377cde3818355146b), including unassisted load and 120 ticks; record downstream failures
4. Run relevant regressions, scoped quality and catalogs; publish a separate draft fix PR only after validation, without merges or stack changes

Completed the implementation and source/native/classic validation. Exact original
Sphere remains blocked by missing genuine lightning.wav; load/120 ticks and a
readiness gain are not claimed. The catalog validator passed; catalog invocation
is blocked by the executor's /proc child-supervisor prerequisite. Publication is
a separate draft fix PR; no merge or native-stack change is authorized.
