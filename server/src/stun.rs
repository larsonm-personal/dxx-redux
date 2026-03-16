//! Self-hosted STUN server with IP allowlisting.
//!
//! Embeds a minimal RFC 5389 STUN Binding Request/Response responder.
//! Only responds to source IPs present in the allowlist (populated by
//! ws_handler on successful authentication, cleared on disconnect).
//!
//! Two listeners run on separate ports so clients can detect symmetric
//! NAT by comparing reflexive addresses from each.

use std::net::{IpAddr, SocketAddr};
use std::sync::Arc;

use dashmap::DashSet;
use tokio::net::UdpSocket;
use tracing::{debug, info, trace, warn};

// STUN constants (RFC 5389)
const STUN_MAGIC: u32 = 0x2112_A442;
const STUN_BINDING_REQUEST: u16 = 0x0001;
const STUN_BINDING_RESPONSE: u16 = 0x0101;
const STUN_ATTR_XOR_MAPPED_ADDRESS: u16 = 0x0020;
const STUN_HEADER_LEN: usize = 20;

/// Run a STUN listener on `addr`. Only responds to source IPs in `allowlist`.
/// Returns the actual bound address (useful when port 0 is specified).
pub async fn run(
    addr: SocketAddr,
    allowlist: Arc<DashSet<IpAddr>>,
) -> Result<SocketAddr, std::io::Error> {
    let socket = UdpSocket::bind(addr).await?;
    let bound = socket.local_addr()?;
    info!(%bound, "STUN listener started");

    tokio::spawn(async move {
        stun_loop(&socket, &allowlist).await;
    });

    Ok(bound)
}

async fn stun_loop(socket: &UdpSocket, allowlist: &DashSet<IpAddr>) {
    let mut buf = [0u8; 512];
    loop {
        let (len, src) = match socket.recv_from(&mut buf).await {
            Ok(r) => r,
            Err(e) => {
                warn!(%e, "STUN recv error");
                continue;
            }
        };

        if !allowlist.contains(&src.ip()) {
            trace!(%src, "STUN: dropped (not in allowlist)");
            continue;
        }

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
            IpAddr::V4(ip) => ip,
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

        // Build STUN Binding Response with XOR-MAPPED-ADDRESS
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
        debug!(%src, "STUN: binding response sent");
    }
}
