# Robot Preview World Scale

## Objective

Make robot preview size reflect model world size for base-game and modded robots without robot-ID-specific adjustments, while retaining a separate camera tier for bosses and reactors that cannot fit the ordinary view.

## Plan

- [ ] Remove the D2 robot-ID scale-reference table and associated camera adjustment.
- [ ] Keep a fixed camera distance within each ordinary/oversized tier so model radius directly controls displayed size.
- [ ] Update preview introspection and integration assertions to verify fixed tier distance and radius-proportional display size.
- [ ] Run scoped quality checks, Android and Windows builds, CTest, and focused emulator tests.

