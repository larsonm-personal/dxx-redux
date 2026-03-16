# NAT Traversal Implementation Plan

## Scope

Implement the first batch of NAT traversal features, covering:
1. Server: relay session cleanup (bug fix)
2. Server: additional integration tests for multi-player NAT flows
3. Client: protocol message types for STUN/connectivity/relay
4. Client: StunClient.kt (STUN query implementation)
5. Client: ConnectivityChecker.kt (candidate pair race)
6. NAT simulation testing strategy and tooling

## Phase 1: Server Relay Cleanup

relay.rs has no session reaping. Sessions leak forever.

Fix: add a periodic cleanup task that removes sessions older than
MAX_RELAY_SESSION_AGE (2 hours). Also remove sessions when all players
disconnect from the WebSocket.

Files:
- server/src/relay.rs: add cleanup_stale_sessions()
- server/src/lib.rs or main.rs: spawn cleanup task on startup
- server/tests/integration.rs: test that stale sessions are reaped

## Phase 2: Server Integration Tests

Add tests for:
- CONNECTIVITY_OK updates lobby player connection_type
- Multi-player (3+) lobby STUN/connectivity flow
- CONNECTIVITY_UPDATE mid-session migration
- Relay session cleanup after timeout

## Phase 3: Client Protocol Messages

Add to NetworkProtocol.kt:
- StunResultMsg (client -> server)
- ConnectivityOkMsg (client -> server)
- ConnectivityUpdateMsg (client -> server)
- PeerCandidatesMsg (server -> client)
- ConnectivityCheckGoMsg (server -> client)
- RelayAssignedMsg (server -> client)
- ConnectionCandidate data class
- CandidatePair data class

Wire into ServerMessage.parse() and MatchmakingService dispatch.

## Phase 4: StunClient.kt

Hand-rolled STUN Binding Request/Response:
- 20-byte request (type=0x0001, length=0, magic=0x2112A442, txn_id)
- Parse XOR-MAPPED-ADDRESS from response
- Query two servers, detect NAT type from port comparison
- Return candidates list

## Phase 5: ConnectivityChecker.kt

Probe-based connectivity test:
- Receive CandidatePair list from server
- For each peer, try pairs in priority order
- 12-byte probe: [magic:4][timestamp:8]
- Echo-based RTT measurement
- 500ms per pair, 3s total
- Fall back to relay if all fail

## Phase 6: NAT Simulation Testing

See detailed analysis in this file below.

---

## NAT Simulation Testing Strategy

### The Challenge

Testing NAT traversal requires peers behind different NAT types.
Real NATs are needed to test the actual UDP holepunching -- mocking
sockets only tests protocol logic, not whether packets actually
traverse NAT devices.

### Approach 1: Docker + iptables (Linux, recommended for CI)

Use Docker containers as isolated network hosts, with iptables rules
simulating different NAT behaviors. This is the most realistic approach.

Setup:
```
                          Host machine
                              |
                   docker bridge (172.20.0.0/16)
                              |
        +------------+--------+---------+------------+
        |            |                  |            |
   [matchmaking]  [nat-a]          [nat-b]      [stun-server]
   172.20.0.2     172.20.0.3       172.20.0.4   172.20.0.5
                     |                  |
              subnet-a (10.0.1.0/24)  subnet-b (10.0.2.0/24)
                     |                  |
                 [client-a]         [client-b]
                 10.0.1.2           10.0.2.2
```

Each "nat" container runs iptables NAT rules. Different rule sets
simulate different NAT types:

**Full cone NAT:**
```bash
iptables -t nat -A POSTROUTING -o eth0 -j MASQUERADE
# Any external host can send to the mapped port
```

**Port-restricted cone NAT:**
```bash
iptables -t nat -A POSTROUTING -o eth0 -j MASQUERADE
# Default Linux conntrack behavior: only responds to addr:port pairs
# that the internal host has sent to
```

**Symmetric NAT:**
```bash
iptables -t nat -A POSTROUTING -o eth0 -j MASQUERADE --random
# --random uses random port allocation per destination
```

This runs headless -- no Android emulator needed. Test clients are
simple Rust or Python UDP programs that implement just the STUN query
and connectivity check logic (same binary protocol, ~100 lines).

**Pros:** Realistic, deterministic, automatable, runs in CI
**Cons:** Linux only, requires Docker, setup complexity

### Approach 2: WSL2 + network namespaces (Windows dev machine)

Use WSL2 on the developer's Windows machine to get Linux network
namespaces. Create isolated network environments with ip netns and
iptables.

```bash
# Create two network namespaces
ip netns add client_a
ip netns add client_b

# Create veth pairs linking each namespace to the host
ip link add veth_a0 type veth peer name veth_a1
ip link set veth_a1 netns client_a
# ... configure IPs, set up NAT with iptables in each namespace

# Run test client in each namespace
ip netns exec client_a ./test_client --stun-server 172.20.0.5 ...
ip netns exec client_b ./test_client --stun-server 172.20.0.5 ...
```

**Pros:** Works on Windows dev machines (via WSL2), no Docker needed
**Cons:** WSL2 networking has quirks, harder to automate than Docker,
network namespaces can be fragile in WSL2

### Approach 3: Simulated NAT at application level (cross-platform)

Build a "NAT simulator" that sits between test clients and the real
network. The simulator is a UDP proxy that applies NAT-like rules to
packets.

```
test_client_a  ->  nat_sim_a (port mapping rules)  ->  network  ->  nat_sim_b  ->  test_client_b
```

The NAT simulator:
- Receives outbound UDP from the test client
- Applies port mapping (static for cone, random for symmetric)
- Forwards to destination with mapped source port
- Receives inbound UDP, checks mapping table, forwards or drops

This can be a ~200-line Rust program. The test clients talk to
localhost ports, and the simulator handles the NAT behavior.

**Pros:** Cross-platform (runs on Windows natively), no Docker/WSL2,
fully deterministic, fast
**Cons:** Doesn't test real kernel NAT behavior, only tests the protocol
logic. Still very useful because it tests the full client-server
message flow including timing.

### Approach 4: Two emulators + VPN (most realistic for Android)

Run two Android emulators, each behind a different VPN or network
configuration. Each runs the actual app.

**Pros:** Tests the real app end-to-end
**Cons:** Very slow, hard to automate, complex setup, requires
two VPN servers or two network interfaces

### Recommendation

Use a layered approach:

1. **Unit tests** (cross-platform, fast):
   - Test STUN message encoding/decoding
   - Test NAT type detection logic
   - Test candidate pair priority sorting
   - Test relay header wrap/unwrap
   - These run on Windows as part of normal build

2. **Application-level NAT simulation** (cross-platform, medium):
   - Build a ~200-line Rust NAT simulator
   - Run it alongside the matchmaking server
   - Test clients connect through the simulator
   - Tests the full message flow: STUN -> candidates -> check -> relay
   - Can test different NAT type combinations
   - Runs on Windows, suitable for dev testing

3. **Docker integration tests** (Linux CI, thorough):
   - Docker Compose config with nat containers
   - Full end-to-end with iptables NAT rules
   - Tests actual packet traversal
   - Runs in GitHub Actions CI on Linux

### Implementation Order

Start with (2) -- the application-level NAT simulator. It provides the
most value for the effort: tests the real protocol flow on the dev's
Windows machine without Docker. Add (3) later for CI.

### NAT Simulator Design (Approach 3)

```rust
// nat_sim.rs -- standalone binary, ~200 lines

struct NatSimulator {
    nat_type: NatType,
    external_port: u16,
    internal_addr: SocketAddr,
    external_socket: UdpSocket,
    internal_socket: UdpSocket,
    // Port mapping table
    mappings: HashMap<SocketAddr, MappedPort>,  // dest -> mapped external port
    reverse: HashMap<u16, SocketAddr>,          // external port -> internal dest
}

enum NatType {
    FullCone,
    PortRestricted,
    Symmetric,
    SymmetricSequential,
}
```

Run two instances:
- nat_sim_a: listens on 127.0.0.1:30000 (internal) and 127.0.0.1:40000 (external)
- nat_sim_b: listens on 127.0.0.1:30001 (internal) and 127.0.0.1:40001 (external)

Test client A sends UDP through nat_sim_a. nat_sim_a maps the port
and forwards. nat_sim_b receives on its external port, checks mapping
rules (accept/reject based on NAT type), forwards to client B.

The STUN server is a simple echo that returns the source address --
when queried through the NAT simulator, it returns the mapped address.

### Test Scenarios

| Scenario                    | NAT A          | NAT B          | Expected Result    |
|-----------------------------|----------------|----------------|-------------------|
| Both full cone              | FullCone       | FullCone       | Direct (srflx)    |
| Cone + restricted           | FullCone       | PortRestricted | Direct (srflx)    |
| Both restricted             | PortRestricted | PortRestricted | Direct (srflx)    |
| One symmetric               | PortRestricted | Symmetric      | Direct (srflx, one side) |
| Both symmetric sequential   | SymSeq         | SymSeq         | Predicted port    |
| Both symmetric random       | Symmetric      | Symmetric      | Relay fallback    |
| STUN server unreachable     | (blocked)      | FullCone       | Relay fallback    |
