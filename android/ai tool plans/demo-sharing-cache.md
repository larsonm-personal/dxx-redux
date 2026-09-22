# Demo sharing cache

## Request

Allow large recorded demos to be shared and replace the misleading unexpired-grants error
Do not compile or run tests while the user's other process is running

## Plan

- Raise the demo export cache budget from 64 MiB to 4 GiB, preserving other roots and 24-hour retention
- Distinguish oversized exports from insufficient remaining cache space with actionable size diagnostics
- Preflight the entire demo export and remove newly staged copies if any file fails
- Log failed demo shares and perform scoped formatting and static diff review only

## Result

- Implemented the 4 GiB demo cache budget and complete-batch capacity/free-space checks
- Failed copy batches remove their newly published generations without touching earlier shares
- Demo share failures use a readable dialog and log file sizes; capacity errors distinguish oversized exports from retained-cache pressure
- Scoped formatting/lint passed for both changed Kotlin files
- Static diff review completed; compilation and tests intentionally not run at the user's request
