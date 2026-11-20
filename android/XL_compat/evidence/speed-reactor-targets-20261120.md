# Speed reactor target list

The original speed.rl2 in the pinned levelpack encodes level layout 23 and one 42-byte reactor trigger record. Its signed target count is -1. The remaining twenty shorts contain unused segment and side slots. The original ReadReactorTriggers reads the count as a short. CTriggerTargets::Read consumes all ten segments and ten sides. Check initializes the effective count to zero and visits no slot when the signed count is negative. See objects/reactor.cpp and main/trigger.cpp at the comparison revision 2576a98b5de310e3ac7c9777df2677d1627b8bae.

Redux now retains the signed count in inventory and writes the original's effective zero into the classic shadow for admitted modern layouts. Negative and positive count controls check both the inventory and the bytes consumed by the native reader. Truncated records and invalid active targets remain covered.

Windows and Linux passed xl_inventory_contract. Both ordinary engines advanced the original Speed mission past its reactor table. Both then stopped on authored team rooms (function 8 in room 4). This repairs a shared reader discrepancy. Speed still fails the startup criterion.
