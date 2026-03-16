# NAT Simulator Implementation Plan

## Goal
Application-level NAT simulator as a separate binary in the server crate.
Used for automated integration tests of the NAT traversal flow.

## Architecture

```
                  matchmaking server (WS on ephemeral port)
                  relay listener (UDP on ephemeral port)
                         |
        +----------------+--------------------+
        |                                     |
   test_client_a (WebSocket)            test_client_b (WebSocket)
        |                                     |
   nat_sim_a (UDP proxy)               nat_sim_b (UDP proxy)
   internal: 127.0.0.1:INT_A           internal: 127.0.0.1:INT_B
   external: 127.0.0.1:EXT_A           external: 127.0.0.1:EXT_B
        |                                     |
        +--------> stun_server <--------------+
                  127.0.0.1:STUN_PORT
```

All components run in the same tokio process. The test spawns:
1. A matchmaking server (reuse TestServer from integration.rs)
2. A mini STUN server (echoes back source address in STUN format)
3. Two NAT simulator instances with configurable NAT type
4. Two test clients that:
   a. Connect to matchmaking WS
   b. Run STUN through their NAT simulator
   c. Exchange candidate info via the server
   d. Run connectivity probes through NAT simulators

## Binary: src/bin/nat_sim.rs

Standalone NAT simulator, also usable from tests via library functions.

Actually -- since everything is in-process for tests, we put the NAT
simulator logic in src/nat_sim.rs as a library module, and optionally
a [[bin]] for standalone use. The integration tests import the module.

## NAT Simulator Module (src/nat_sim.rs)

Core types:
- NatType enum: FullCone, PortRestricted, Symmetric, SymmetricSequential
- NatSimulator struct: runs async, manages port mappings, proxies UDP

The simulator binds two UDP sockets:
- internal_socket: test client sends to this
- external_socket: forwards packets to the real network

Mapping behavior per NAT type:
- FullCone: one external port per internal (src_ip, src_port). Any
  external host can send back to that port.
- PortRestricted: same mapping, but only allows inbound from (addr, port)
  pairs the internal host has previously sent to.
- Symmetric: different external port per (dest_ip, dest_port). Only
  allows inbound from the specific dest that was sent to.
- SymmetricSequential: like Symmetric but ports increment sequentially
  from a base.

## Mini STUN Server (src/nat_sim.rs)

Minimal STUN server (~30 lines): receives Binding Request, responds
with XOR-MAPPED-ADDRESS containing the sender's source address. This
is all a STUN server does for our purposes.

## Test Scenarios (as integration tests)

1. both_full_cone: Both behind full cone NAT -> direct srflx connection
2. cone_and_restricted: Full cone + port restricted -> direct srflx
3. both_symmetric_sequential: Both symmetric sequential -> predicted port
4. both_symmetric_random: Both symmetric random -> relay fallback
5. stun_unreachable: NAT sim drops STUN -> unknown NAT + relay
6. one_symmetric_one_cone: Asymmetric NAT -> srflx works

## Files to Create/Modify

- server/Cargo.toml: add [[bin]] for nat_sim
- server/src/nat_sim.rs: NAT simulator + mini STUN server module
- server/src/bin/nat_sim.rs: standalone binary entry point
- server/src/lib.rs: add pub mod nat_sim
- server/tests/nat_sim_tests.rs: integration tests using the NAT sim
- server/run_nat_tests.ps1: PowerShell script to run NAT tests
