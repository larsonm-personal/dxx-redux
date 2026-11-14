# Complete nightly coverage and authorized revision selection

- Base: current cmake bf40de9bc3b85c2279fe8163c621bb6c052dec12; preserve public PR samples
- Resolve latest reachable same-repository revision from GitHub activity, using immutable actor ID 265118235 and server timestamp, with complete cursor pagination and a stable branch/activity snapshot
- Partition the complete admitted unattended catalog into bounded independent shards each night, including online opt-ins; retain explicit unavailable and manual exclusions
- Keep controller scripts pinned to workflow SHA, acquire private inputs before running candidate code, remove checkout credentials, and publish bounded receipts only for private lanes
- Require actual owner/leaf receipts for every selected shard; missing results fail qualification
- Test selection, security boundaries, sharding and coverage; run scoped quality and catalog checks, publish draft PR without merging or pushing cmake
- Deployment requires merge into cmake, the scheduler's default branch; private assets retain existing retail-ci configuration requirements
