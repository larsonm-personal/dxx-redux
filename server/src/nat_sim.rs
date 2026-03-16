//! Application-level NAT simulator for testing NAT traversal.
//!
//! Each port mapping gets its own external UDP socket, so STUN servers
//! see distinct source ports for different mappings (critical for
//! symmetric NAT simulation).

use std::collections::HashMap;
use std::net::SocketAddr;
use std::sync::Arc;

use tokio::net::UdpSocket;
use tokio::sync::Mutex;
use tokio::task::JoinHandle;
use tracing::{debug, trace};

// ---------------------------------------------------------------------------
// NAT types
// ---------------------------------------------------------------------------

/// NAT behavior to simulate.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum NatType {
    /// One external port per (internal src). Any external host can send to it.
    FullCone,
    /// One external port per (internal src). Only hosts+ports previously sent
    /// to may send back.
    PortRestricted,
    /// Different external port per (internal src, dest addr, dest port).
    /// Only the specific dest may reply.
    Symmetric,
    /// Like Symmetric but ports increment sequentially from a base.
    SymmetricSequential,
}

// ---------------------------------------------------------------------------
// NAT simulator
// ---------------------------------------------------------------------------

/// A mapping from an internal source to a dedicated external socket.
struct PortMapping {
    external_socket: Arc<UdpSocket>,
    /// Remote addrs allowed to send inbound through this mapping.
    allowed_remotes: Vec<SocketAddr>,
}

/// Internal state behind a Mutex.
struct NatState {
    nat_type: NatType,
    mappings: HashMap<String, PortMapping>,
    /// Address of the first internal client seen (set on first outbound).
    internal_client_addr: Option<SocketAddr>,
    /// Next port to use for SymmetricSequential mappings.
    next_seq_port: u16,
}

impl NatState {
    fn mapping_key(nat_type: NatType, _src: SocketAddr, dest: SocketAddr) -> String {
        match nat_type {
            NatType::FullCone | NatType::PortRestricted => "default".into(),
            NatType::Symmetric | NatType::SymmetricSequential => {
                format!("{}:{}", dest.ip(), dest.port())
            }
        }
    }
}

/// Handle to a running NAT simulator.
pub struct NatSimHandle {
    /// The address the internal (test) client should send to.
    pub internal_addr: SocketAddr,
    /// The external IP (127.0.0.1). Actual mapped ports are per-mapping
    /// and discovered via STUN.
    pub external_ip: std::net::IpAddr,
    tasks: Arc<Mutex<Vec<JoinHandle<()>>>>,
}

impl NatSimHandle {
    pub fn abort(&self) {
        if let Ok(tasks) = self.tasks.try_lock() {
            for t in tasks.iter() {
                t.abort();
            }
        }
    }
}

/// Start a NAT simulator.
///
/// `base_external_port`: for SymmetricSequential, the starting port to
/// allocate from. Pass 0 to pick one automatically.
pub async fn start_nat(nat_type: NatType, base_external_port: u16) -> NatSimHandle {
    let internal_socket = UdpSocket::bind("127.0.0.1:0").await.unwrap();
    let internal_addr = internal_socket.local_addr().unwrap();

    // Pick a base port for SymmetricSequential if not specified
    let seq_base = if base_external_port != 0 {
        base_external_port
    } else {
        // Bind a throwaway socket to get a free port, then use nearby range
        let tmp = UdpSocket::bind("127.0.0.1:0").await.unwrap();
        tmp.local_addr().unwrap().port().wrapping_add(200)
    };

    let state = Arc::new(Mutex::new(NatState {
        nat_type,
        mappings: HashMap::new(),
        internal_client_addr: None,
        next_seq_port: seq_base,
    }));

    let tasks: Arc<Mutex<Vec<JoinHandle<()>>>> = Arc::new(Mutex::new(Vec::new()));
    let tasks_clone = Arc::clone(&tasks);
    let internal_socket = Arc::new(internal_socket);

    let main_task = tokio::spawn(nat_outbound_loop(
        Arc::clone(&internal_socket),
        Arc::clone(&state),
        tasks_clone,
    ));
    tasks.lock().await.push(main_task);

    NatSimHandle {
        internal_addr,
        external_ip: std::net::IpAddr::V4(std::net::Ipv4Addr::LOCALHOST),
        tasks,
    }
}

/// Main loop: reads from internal socket, creates mappings, sends outbound.
async fn nat_outbound_loop(
    internal_socket: Arc<UdpSocket>,
    state: Arc<Mutex<NatState>>,
    tasks: Arc<Mutex<Vec<JoinHandle<()>>>>,
) {
    let mut buf = [0u8; 2048];
    loop {
        let (len, src) = match internal_socket.recv_from(&mut buf).await {
            Ok(r) => r,
            Err(_) => continue,
        };
        if len < 6 {
            debug!("NAT sim: packet too short from internal client");
            continue;
        }
        let dest_ip = std::net::Ipv4Addr::new(buf[0], buf[1], buf[2], buf[3]);
        let dest_port = u16::from_be_bytes([buf[4], buf[5]]);
        let dest = SocketAddr::new(dest_ip.into(), dest_port);
        let payload = buf[6..len].to_vec();

        let mut st = state.lock().await;
        st.internal_client_addr = Some(src);
        let key = NatState::mapping_key(st.nat_type, src, dest);

        // Create mapping with a new external socket if needed
        if !st.mappings.contains_key(&key) {
            let ext_sock = if st.nat_type == NatType::SymmetricSequential {
                // Bind to a specific sequential port
                let port = st.next_seq_port;
                st.next_seq_port = st.next_seq_port.wrapping_add(1);
                Arc::new(
                    UdpSocket::bind(format!("127.0.0.1:{port}"))
                        .await
                        .unwrap_or_else(|_| panic!("failed to bind port {port}")),
                )
            } else {
                Arc::new(UdpSocket::bind("127.0.0.1:0").await.unwrap())
            };
            let ext_addr = ext_sock.local_addr().unwrap();
            trace!(nat_type = ?st.nat_type, %src, %dest, port = ext_addr.port(), "NAT: new mapping");

            // Spawn inbound handler for this mapping's external socket
            let inbound = tokio::spawn(nat_inbound_handler(
                Arc::clone(&ext_sock),
                Arc::clone(&state),
                Arc::clone(&internal_socket),
                key.clone(),
            ));
            tasks.lock().await.push(inbound);

            st.mappings.insert(
                key.clone(),
                PortMapping {
                    external_socket: ext_sock,
                    allowed_remotes: Vec::new(),
                },
            );
        }
        let mapping = st.mappings.get_mut(&key).unwrap();

        if !mapping.allowed_remotes.contains(&dest) {
            mapping.allowed_remotes.push(dest);
        }

        let ext_sock = Arc::clone(&mapping.external_socket);
        drop(st);

        if let Err(e) = ext_sock.send_to(&payload, dest).await {
            debug!(%e, %dest, "NAT sim: external send failed");
        }
    }
}

/// Per-mapping inbound handler: reads from one external socket, forwards
/// to internal client with filtering and 6-byte source header.
async fn nat_inbound_handler(
    ext_socket: Arc<UdpSocket>,
    state: Arc<Mutex<NatState>>,
    internal_socket: Arc<UdpSocket>,
    mapping_key: String,
) {
    let mut buf = [0u8; 2048];
    loop {
        let (len, src) = match ext_socket.recv_from(&mut buf).await {
            Ok(r) => r,
            Err(_) => break,
        };
        let payload = buf[..len].to_vec();

        let st = state.lock().await;
        let mapping = match st.mappings.get(&mapping_key) {
            Some(m) => m,
            None => continue,
        };
        let allowed = match st.nat_type {
            NatType::FullCone => true,
            NatType::PortRestricted | NatType::Symmetric | NatType::SymmetricSequential => {
                mapping.allowed_remotes.contains(&src)
            }
        };
        let internal_addr = match st.internal_client_addr {
            Some(a) => a,
            None => continue,
        };
        drop(st);

        if allowed {
            let mut response = Vec::with_capacity(6 + payload.len());
            if let std::net::IpAddr::V4(ip) = src.ip() {
                response.extend_from_slice(&ip.octets());
            } else {
                continue;
            }
            response.extend_from_slice(&src.port().to_be_bytes());
            response.extend_from_slice(&payload);
            let _ = internal_socket.send_to(&response, internal_addr).await;
        } else {
            trace!(%src, key = %mapping_key, "NAT sim: inbound dropped (not allowed)");
        }
    }
}

// ---------------------------------------------------------------------------
// Mini STUN server
// ---------------------------------------------------------------------------

/// STUN constants (RFC 5389)
const STUN_MAGIC: u32 = 0x2112_A442;
const STUN_BINDING_REQUEST: u16 = 0x0001;
const STUN_BINDING_RESPONSE: u16 = 0x0101;
const STUN_ATTR_XOR_MAPPED_ADDRESS: u16 = 0x0020;
const STUN_HEADER_LEN: usize = 20;

/// Handle to a running mini STUN server.
pub struct StunServerHandle {
    pub addr: SocketAddr,
    task: tokio::task::JoinHandle<()>,
}

impl StunServerHandle {
    pub fn abort(&self) {
        self.task.abort();
    }
}

/// Start a minimal STUN server that responds to Binding Requests with
/// XOR-MAPPED-ADDRESS containing the sender's source address.
pub async fn start_stun_server() -> StunServerHandle {
    let socket = UdpSocket::bind("127.0.0.1:0").await.unwrap();
    let addr = socket.local_addr().unwrap();

    let task = tokio::spawn(async move {
        let mut buf = [0u8; 512];
        loop {
            let (len, src) = match socket.recv_from(&mut buf).await {
                Ok(r) => r,
                Err(_) => continue,
            };
            if len < STUN_HEADER_LEN {
                continue;
            }

            let msg_type = u16::from_be_bytes([buf[0], buf[1]]);
            if msg_type != STUN_BINDING_REQUEST {
                continue;
            }
            let magic = u32::from_be_bytes([buf[4], buf[5], buf[6], buf[7]]);
            if magic != STUN_MAGIC {
                continue;
            }
            let txn_id = &buf[8..20];

            let ip = match src.ip() {
                std::net::IpAddr::V4(ip) => ip,
                _ => continue,
            };
            let xor_port = src.port() ^ ((STUN_MAGIC >> 16) as u16);
            let xor_ip: [u8; 4] = {
                let ip_bytes = ip.octets();
                let magic_bytes = STUN_MAGIC.to_be_bytes();
                [
                    ip_bytes[0] ^ magic_bytes[0],
                    ip_bytes[1] ^ magic_bytes[1],
                    ip_bytes[2] ^ magic_bytes[2],
                    ip_bytes[3] ^ magic_bytes[3],
                ]
            };

            let attr_value_len: u16 = 8;
            let msg_len: u16 = 4 + attr_value_len;
            let mut resp = Vec::with_capacity(32);

            resp.extend_from_slice(&STUN_BINDING_RESPONSE.to_be_bytes());
            resp.extend_from_slice(&msg_len.to_be_bytes());
            resp.extend_from_slice(&STUN_MAGIC.to_be_bytes());
            resp.extend_from_slice(txn_id);

            resp.extend_from_slice(&STUN_ATTR_XOR_MAPPED_ADDRESS.to_be_bytes());
            resp.extend_from_slice(&attr_value_len.to_be_bytes());
            resp.push(0x00); // reserved
            resp.push(0x01); // family: IPv4
            resp.extend_from_slice(&xor_port.to_be_bytes());
            resp.extend_from_slice(&xor_ip);

            let _ = socket.send_to(&resp, src).await;
        }
    });

    StunServerHandle { addr, task }
}

// ---------------------------------------------------------------------------
// STUN client helper (for test clients running through NAT sim)
// ---------------------------------------------------------------------------

/// Send a STUN Binding Request through a NAT simulator and parse the
/// reflexive address from the response.
pub async fn stun_query_through_nat(
    socket: &UdpSocket,
    nat_addr: SocketAddr,
    stun_server_addr: SocketAddr,
) -> Option<SocketAddr> {
    let txn_id: [u8; 12] = rand::random();
    let mut request = Vec::with_capacity(6 + STUN_HEADER_LEN);

    // NAT sim header: [dest_ip:4][dest_port:2]
    match stun_server_addr.ip() {
        std::net::IpAddr::V4(ip) => request.extend_from_slice(&ip.octets()),
        _ => return None,
    }
    request.extend_from_slice(&stun_server_addr.port().to_be_bytes());

    // STUN header
    request.extend_from_slice(&STUN_BINDING_REQUEST.to_be_bytes());
    request.extend_from_slice(&0u16.to_be_bytes()); // length = 0
    request.extend_from_slice(&STUN_MAGIC.to_be_bytes());
    request.extend_from_slice(&txn_id);

    socket.send_to(&request, nat_addr).await.ok()?;

    // Receive response (with 6-byte NAT sim source header prepended)
    let mut buf = [0u8; 256];
    let timeout = tokio::time::timeout(
        std::time::Duration::from_secs(2),
        socket.recv_from(&mut buf),
    )
    .await;
    let (len, _from) = timeout.ok()?.ok()?;

    // Strip 6-byte NAT sim header
    if len < 6 + STUN_HEADER_LEN {
        return None;
    }
    let stun_data = &buf[6..len];

    // Parse STUN response
    let msg_type = u16::from_be_bytes([stun_data[0], stun_data[1]]);
    if msg_type != STUN_BINDING_RESPONSE {
        return None;
    }
    let resp_magic = u32::from_be_bytes([stun_data[4], stun_data[5], stun_data[6], stun_data[7]]);
    if resp_magic != STUN_MAGIC {
        return None;
    }
    if stun_data[8..20] != txn_id {
        return None;
    }

    let msg_len = u16::from_be_bytes([stun_data[2], stun_data[3]]) as usize;
    let attr_end = STUN_HEADER_LEN + msg_len;
    let mut pos = STUN_HEADER_LEN;
    while pos + 4 <= attr_end && pos + 4 <= stun_data.len() {
        let attr_type = u16::from_be_bytes([stun_data[pos], stun_data[pos + 1]]);
        let attr_len = u16::from_be_bytes([stun_data[pos + 2], stun_data[pos + 3]]) as usize;
        if attr_type == STUN_ATTR_XOR_MAPPED_ADDRESS && attr_len >= 8 {
            let val = &stun_data[pos + 4..pos + 4 + attr_len];
            if val[1] != 0x01 {
                return None; // not IPv4
            }
            let xor_port = u16::from_be_bytes([val[2], val[3]]);
            let port = xor_port ^ ((STUN_MAGIC >> 16) as u16);
            let magic_bytes = STUN_MAGIC.to_be_bytes();
            let ip = std::net::Ipv4Addr::new(
                val[4] ^ magic_bytes[0],
                val[5] ^ magic_bytes[1],
                val[6] ^ magic_bytes[2],
                val[7] ^ magic_bytes[3],
            );
            return Some(SocketAddr::new(ip.into(), port));
        }
        pos += 4 + ((attr_len + 3) & !3);
    }
    None
}
