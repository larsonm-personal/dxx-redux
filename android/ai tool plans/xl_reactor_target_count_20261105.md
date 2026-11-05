# XL reactor target count qualification

- Compare unchanged Boiling Point control2.rl2 against pinned CTriggerTargets::Read/Check
- Preserve authored count and qualify modern fixed-slot capping, keeping negative counts, invalid targets and truncated records rejected
- Translate the effective count before the classic loader; do not change classic loaders or originals
- Test boundary counts, order/duplicates, malformed slots, private translation and exact original inspection against the pinned reader
- Run relevant regressions and publish a separate draft only after validation

OOF SPCL/RANI remains an independent original load blocker; no readiness gain is claimed
