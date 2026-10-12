# Runtime-generated ordinary save persistence

- Capture D1 and D2 public flight checkpoints and an active D1 door checkpoint using ordinary controls
- Write compact lossless engine object/world streams around save, restore and both continuation branches
- Gate exact documented immediate persistence plus flight inventory, health, outcomes, RNG state and per-step SIM draw deltas
- Preserve every raw observation and strict difference; full trajectories remain unqualified because ordinary saves rebase game time
- Add fail-closed admission rejection tests, isolated runner, catalog registration and public CI
- Build both native fixtures, exercise pinned public assets, run scoped formatting and both catalog checks
- Do not change engine serialization, clock behavior, retail fixtures or immutable capture goldens
