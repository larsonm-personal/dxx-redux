# Simulation batch failures

1. Bucket the 20260904_233155 infrastructure failures and inspect native crash paths
2. Reproduce representative crashes and fix the shared cause without weakening routing
3. Rebuild and verify focused mission runs and native tests, leaving unrelated regression data untouched

## Results

- Batch completed 1794 levels with 35 infrastructure failures: 23 native crashes, 6 other engine errors, 6 process timeouts
- move_towards_outside used a 200-element stack array after safety-point insertion could expand the path beyond 200 elements
- Allocate the smoothing scratch buffer for the actual point count and free it after copying, without truncating paths or changing movement rules
- All 23 native crash cases reproduced as controlled simulation timeouts after the fix, with valid result JSON
- Four long-path levels passed repeated-run determinism checks, including revodrav 18/23 and bratmaze 1
- Simulation staging now strips DOS EOF markers from descriptors, matching metadata staging and leaving binary assets unchanged
- beta1.rdl now confirms completion; iceout.rdl now loads and returns a controlled simulation timeout
- Windows D2 build, all 47 configured native tests, runner/staging tests and scoped quality checks passed
- No checked-in regression JSON was replaced

## Remaining failures

- Missing saturn15.rdl asset
- alpha004.rdl requested outside loaded mission range
- junine.rdl geometry assertion in get_seg_masks via sphere_intersects_wall
- dontpnic secret level secondary weapon order-list error
- Six process timeouts: BRDECON.rdl, neural-i.rdl, twisty.rdl, eris0001.rdl, shipport.rdl, megaloma.rdl
- Routing stalls underlying the now-controlled native-crash cases remain separate simulation failures
