# XL channel allocation and saved continuation

The owned named-audio runtime currently fills the first free slot and refuses
new voices when all 128 slots are occupied. XL scans from a retained cursor,
then replaces a voice using sound class, persistence and effective volume.
Establish that behavior before introducing ordinary replacements and spatial
sound objects into the owned XL pool. Redux retains its existing alternate.

## Work

1. Execute pinned `SoundClassCount`, `FindFreeChannel` and `StartSound` with
   independent channel/storage adapters. Cover cursor wrap, available slots,
   the half-pool class threshold, persistent priority, volume ties, failed starts
   and zero versus negative repeat requests. Keep object cleanup outside the
   named-only runtime claim.
2. Use the source allocation policy in the owned voice kernel. Named requests
   have uniform volume; negative repetitions are persistent, while zero
   repetitions loop without acquiring that priority. Retain serial identities
   when slots are reused and preserve the allocation cursor when sounds stop.
3. Save and validate the cursor and captured effective volume with live voice
   state. Reconstruct class and persistence from the owned authored action.
   Replace the pre-release trailer
   schema directly; do not add compatibility readers.
4. Extend the existing source/native/Android owners with full-pool replacement,
   actual output, saved continuation, rewind, disk-through-stock and malformed
   cursor controls. Keep meaningful single-voice and Redux controls. Avoid new
   test owners or duplicated fixture builds.
5. Run affected source/public/private contracts and serial Android integration,
   scoped formatting and required catalogs. Publish the reviewed increment on
   the current stack and monitor CI without merging.

Ordinary replacement playback, ambient ownership/spatialization, mixed
Redux/XL channel exhaustion and original campaign acceptance remain separate
requirements. No mission receives qualification from an allocator comparison.
Do not navigate levels or modify original distributions or the main checkout.

## Completed validation

The pinned source owner passes 5,723 allocation comparisons. Complete public
owners pass 43 Windows and 46 Linux controls, and the private owner passes all
103 selected controls in 351.339 seconds. Both native layouts and the focused
Linux audio owner pass the final continuation assertion, which distinguishes
a lost cursor by the next chosen voice.

Android passes the existing 73-step full-pool/single-voice owner, 65-step folder
owner and 27-step replacement-rejection control. Full-pool checks include real
PCM, rewind, disk-through-stock and resumed replacement decisions. Protected
data is restored. Formatting and both catalogs pass; no new owner or increased
timeout was needed. Detailed receipts and the explicit application-volume
policy difference are in [audio-channels.md](../../XL_compat/evidence/audio-channels.md).
