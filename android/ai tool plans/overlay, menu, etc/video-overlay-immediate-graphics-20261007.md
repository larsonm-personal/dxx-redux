# Immediate Video Info graphics edits

Apply Video Info edits at the next safe game-thread render boundary, protecting the accepted settings before GL work. Keep the existing 2500 ms shared confirmation quiet period and five-second confirmation countdown

1. Reuse durable preview ownership for live Video Info edits, allowing further edits during the quiet period
2. Keep the Android watchdog independent of rendering without opening a modal while editing
3. Extend the existing Video Info integration test to require applied settings before confirmation, then verify both engines, recovery, scoped quality and the Android native build

Status: implementing
