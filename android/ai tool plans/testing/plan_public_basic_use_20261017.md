# Public basic-use smoke

## Scope

Extend the existing public import owner instead of introducing a second device framework.
Exercise normal launcher buttons and untouched pilot settings on a cleared disposable
emulator: native D1, D1 in D2, then native D2 with the retained D2 pilot.

- Open/cancel the native Game Menu twice with Escape
- Open automap with Tab and close with Escape
- Observe each modal transition and require live, unpaused level-one gameplay after return
- Preserve correlated durable automation receipts and report observed settings rather
  than prescribing defaults or editing pilot files
- Add negative profile controls for missing input/wait/assertion and paused returns

The explicit `-PublicBasicUse` profile runs the existing empty/D1/D1-in-D2/D2 phases,
public imports and DocumentsUI cancellation. It does not run mission replacement,
custom mission save/restore or launcher resume. Those remain in full first-play.
Both profiles suppress graphics onboarding through the existing runner. Ordinary
keyboard inputs exercise native controls, not physical touch hit targets.

## Budget and ownership

Register `test_demo_import_public_basic_use` with the existing `test_demo_import`
owner and a 720-second master timeout. No new standalone JSON scripts are added.
Nightly fixture admission and campaign selection are separate coordinated changes.
Two read-only shared introspection fields expose observed pilot ship/missile colors.
No private assets or forced gameplay/settings variables are used.

## Verification

Run the automation/catalog/profile controls and scoped formatting in the existing
hosted tooling lane; local PowerShell is unavailable for this task. Qualify the
independent basic-use profile in the existing public Android lane. Full first-play
qualification is separate and must not be inferred from basic-use PASS.
