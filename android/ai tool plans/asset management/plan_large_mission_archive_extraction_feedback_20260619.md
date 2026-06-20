# Large mission archive extraction feedback

## Goal
- Make large mission archive imports visibly distinct from plain stored archive imports while extraction is running and after it completes

## Plan
- [x] Report durable extraction progress with clear user-facing labels
- [x] Show completion text that says the level pack was extracted and cached for faster launches
- [x] Keep ordinary stored mission ZIP imports labeled as ordinary imports
- [x] Run focused tests, assemble, and scoped code quality

## Notes
- Durable extraction now reports determinate byte progress while archive entries are unpacked
- The in-progress launcher message says the level pack is being cached for faster launches and shows copied bytes when a total is known
- Completion status for extracted mission archives uses an extracted-cache success block instead of the ordinary import line
- Nested Rebirth child extraction uses progress labels too
