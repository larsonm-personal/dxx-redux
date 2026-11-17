# Route failure evidence follow-up

Run 43936883066 tested 25ec19e and produced 125 infrastructure failures plus seven changed non-ok route outcomes
The seven are six Countd2 timeout-to-route_mismatch changes and one megalo timeout-to-failed change
No previously successful oracle is represented in those seven

## Narrow fix

Read fixed categories from the route owner's infrastructure summary and bounded native process sidecars/logs before the retail boundary removes private files
Preserve expected runtime status for outcome differences
Do not change route admission, oracle comparisons, sampling, or committed baselines

## Validation

Synthetic owner-layout process/log fixtures, privacy filtering, resanitization and corpus tests
Full sweep suite and CI checks where available
Pinned private-asset reruns are still required to establish the causes behind the 125 infrastructure failures

## Current-base replacement

Forward-port PR #546 commit fcce63818148a7fbf1a45ac223c5c046c7df3666 onto
cmake eba98b696 after #543, #545 and #547. Resolve the adjacent test insertion
conflict by retaining both the existing public-online assertion privacy test and
the route diagnostics privacy test. No branch merges or published-history edits.
The original PR remains open for coordination. Corpus 26 and retail 23 tests pass.
