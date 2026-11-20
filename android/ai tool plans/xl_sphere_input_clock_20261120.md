# Earned input-clock comparison, November 20

The Sphere morph fixture exposed a one-unit player Z difference and small robot-position differences after 100 ms of otherwise identical warm and cold flight. Windows and Linux reproduced the difference when starting from the same earned checkpoint. At 60 Hz the developer player runner divides 65,536 fixed-point units across the frames of each second. The warm run had a remainder of 12 at the save. The fresh process started at zero. Both branches reached the same game time, but the individual frame lengths arrived in a different order.

An earned checkpoint receipt now records its SHA-256, simulation rate and current division remainder. A cold input plan can request that exact clock when it compares the next controls to a warm run. The runner checks the save hash, rate, integer range and whether the rate can produce the requested remainder. Existing plans start with their previous schedule unless they request a saved clock.

On both systems, carrying the saved remainder made all 18 observed fields agree, including player position, nearby object positions, the morph slot, XL state and simulation RNG. The registered fixture creates a second, independently nonzero remainder by advancing an earned Sphere checkpoint by one 60 Hz frame. It saves, advances 100 ms, then runs those same frames from the cold save and compares the two snapshots. Run it with the authentic Sphere inputs configured for the existing gold-key fixture.

~~~sh
ctest --test-dir <contracts-build> --output-on-failure -R "^xl_sphere_input_clock_restore$"
~~~

These tests measure the developer flight runner. Pumo levels 8 and 14 still have separate warm detailed-state reports, and the Android startup capture has 1,500 extra effects RNG draws. [Input, binary and comparison receipts](../XL_compat/evidence/sphere-input-clock-20261120.json)
