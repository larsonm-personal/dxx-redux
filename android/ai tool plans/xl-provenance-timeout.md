# Bounded XL provenance transport recovery

The selector in run 42143195174 failed before candidate execution when its
commit-to-PR association GET exceeded the existing 30-second subprocess limit.
The earlier same-head run 42143159685 passed actual XL smoke. Both used trusted
controller 42f2ba502 and candidate 1475dc53c; readiness remains a separate failure.

Retry a timed-out read once after a one-second delay. Keep the 30-second attempt
limit, workflow deadlines, all provenance validation and readiness semantics.
Discard partial timeout output. Do not retry API rejection, invalid JSON or
invalid provenance, and propagate an exhausted timeout without admission.

Validate recovery and exhaustion through the automatic verifier, including empty,
ambiguous and truncated associations, closed PRs, moved heads/refs and foreign
repositories. Run the existing controller, publisher and host-policy regressions
and scoped formatting before publishing a separate draft fix PR. The trusted
controller cannot use this correction until separately approved and merged; a
candidate's passing workflow does not establish deployed retry behavior.
