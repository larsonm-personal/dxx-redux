# BR-0253 fail Android startup when selected PhysicsFS content cannot mount

## Plan

- [x] Read BR-0253, repository instructions, and review process
- [x] Trace selected PhysicsFS content from launcher configuration through paired D1/D2 startup
- [x] Make required selected-content mount failures abort startup with an actionable diagnostic
- [x] Add focused automated coverage for successful, absent, and failed selected mounts
- [x] Run scoped code quality, native tests, and relevant Android builds
- [x] Archive BR-0253 with exact validation evidence
