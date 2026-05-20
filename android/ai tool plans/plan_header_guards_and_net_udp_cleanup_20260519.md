## Goal

- add missing include guards to the unguarded project headers, starting with `d1/main/net_udp.h` and `d2/main/net_udp.h`
- remove the shared net helper workaround that avoided directly depending on the real UDP packet types because those headers were unguarded
- continue with the next small D1/D2 diff-minimization slice after the header baseline is fixed

## Plan

- [completed] redo the header audit with a full-file tracked-header scan, remove the shallow-scan false positives, and add guards only to the truly unguarded tracked headers (`d1/arch/win32/include/resource.h`, `d1/xmodel/strfunc.h`, `d2/arch/win32/include/resource.h`, `d2/xmodel/strfunc.h`)
- [completed] simplify the shared `net_udp` helper surface to use the real guarded UDP header/types instead of the workaround-shaped interfaces
- [completed] run focused Android and Windows validation plus the quick suite
- [completed] continue with the next nearby deduplication slice by moving the identical `net_udp_objnum_is_past` logic into the shared helper layer