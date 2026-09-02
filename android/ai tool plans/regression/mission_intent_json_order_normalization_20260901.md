# Mission intent JSON order normalization

## Goal

Make every mission metadata generation path serialize `mission_intent` at one
canonical location so alternating host and Android regenerations do not churn
otherwise unchanged regression files.

## Phases

- [x] Compare the host, Android, and shared JSON normalization property order
- [x] Define and implement one canonical top-level mission property order
- [x] Add regression coverage for both object and array mission outputs
- [x] Run focused regeneration comparisons, quality checks, and relevant tests

## Result

- The shared mission metadata normalizer places `mission_intent` immediately
  after `mission_filename`, independent of input property order.
- Host-style and Android-style object output normalize byte-identically.
- All 140 checked mission metadata files pass the canonical normalizer, and all
  384 mission records pass the structured intent and property-order regression.
