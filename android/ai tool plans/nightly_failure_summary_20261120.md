# Nightly failure details

The November 20 sweep's five non-provisioning red Linux shards reported corpus input exclusions, while Android recorded three owner failures and one interrupted attempt. The private-input lane's exported receipts intentionally omit raw logs. Report failed owners and the available safe diagnostics without exposing raw output or changing any sampling, gating, or retry decision.

1. Use bounded, revision-checked owner diagnostics and fixed failure categories for public Android graphics and import tests.
2. Show up to eight current failure receipts and counts by known input blocker in the compact nightly summary. Keep historical state and raw evidence out of the report.
3. Test the compact renderer with existing receipts and a synthetic owner failure. Leave Gradle provisioning as a separate change.
