# GQR-0301 mouse-mode Extreme Action capability, 2026-10-14

Diagnosis-only remediation plan for GQF-0315. No implementation or runtime acceptance.

Current TouchEditorPage offers Extreme Action for all stick modes and saves its configured actions. Actual mouse begin/move/drain paths only emit mouse axes; updateStickExtremeActions is called only from ordinary updateStickFromTouch. A mouse-mode stick therefore cannot activate its saved extreme action through its own mouse gesture.

Define the mouse over-travel coordinate contract explicitly before wiring dispatch: physical reference origin, stick radius, signed X/Y travel, edge-only motion, pointer transfer and re-entry. Preserve the intended advertised capability, ordinary mouse axes, edge ramp/standoff/origin policy and upward-positive ordinary extreme behavior. Do not use accumulated velocity, acceleration history or arbitrary clipped visual bands as a physical radius. Do not hide the advertised option merely to satisfy current tests.

Keep dispatch in shared Android code with explicit HOLD versus PULSE_ON_ENTER hysteresis, binding admission and per-action source identity. Reuse existing routing, D1 admission and retirement owners without duplicating their fixes. Preserve source-tag mixing, current mailbox transitions and narrow paired native hooks.

Qualify the actual editor-save-load/View mouse down/move/drain and paired D1/D2 frame-visible behavior, with enabled/disabled X/Y positive/negative, threshold/release boundaries, held/pulse rearm, ordinary/edge-only motion, reset/cancel/steal/automap/variant/layout/deactivation and concurrent controller/touch holds. Pure helper assertions alone are insufficient. No compatibility reader, migration or new engine-format policy.

Impact47 MEDIUM: H23 material input correctness; M0 no inherited reduction; B7 shared paired-game touch subsystem; C10 explicit saved configuration and absent runtime route; R7 localized Android contract/dispatch repair with moderate platform validation. Canonical admission occurs with complete chunk0434 publication, not merely this plan.
