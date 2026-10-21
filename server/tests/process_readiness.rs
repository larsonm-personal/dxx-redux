//! Owned loopback process rehearsal, without inherited deployment configuration
use std::process::{Child, Command, Stdio};
use std::time::{Duration, Instant};

struct OwnedChild(Child);
impl Drop for OwnedChild {
    fn drop(&mut self) {
        let _ = self.0.kill();
        let _ = self.0.wait();
    }
}
fn start(config: &serde_json::Value, root: &std::path::Path) -> OwnedChild {
    let path = root.join("config.json");
    std::fs::write(&path, serde_json::to_vec(config).unwrap()).unwrap();
    OwnedChild(
        Command::new(env!("CARGO_BIN_EXE_dxx-matchmaking"))
            .env_clear()
            .env("CONFIG_FILE", path)
            .current_dir(root)
            .stdin(Stdio::null())
            .stdout(Stdio::null())
            .stderr(Stdio::null())
            .spawn()
            .unwrap(),
    )
}
fn config() -> serde_json::Value {
    serde_json::json!({
        "ws_listen_addr":"127.0.0.1:0", "http_listen_addr":"127.0.0.1:0",
        "relay_listen_addr":"127.0.0.1:0", "stun_listen_addr":"127.0.0.1:0",
        "stun_listen_addr_alt":"127.0.0.1:0", "admin_http_listen_addr":"127.0.0.1:0",
        "stun_public_addrs":"127.0.0.1:3478,127.0.0.1:3479",
        "db_path":"synthetic.db", "log_dir":"", "skip_gpgs_verify":false
    })
}
#[test]
fn process_exits_on_required_udp_bind_failure() {
    for listener in [
        "relay_listen_addr",
        "stun_listen_addr",
        "stun_listen_addr_alt",
    ] {
        let root = tempfile::tempdir().unwrap();
        let occupied = std::net::UdpSocket::bind("127.0.0.1:0").unwrap();
        let mut cfg = config();
        cfg[listener] = occupied.local_addr().unwrap().to_string().into();
        let mut process = start(&cfg, root.path());
        let deadline = Instant::now() + Duration::from_secs(5);
        loop {
            if let Some(status) = process.0.try_wait().unwrap() {
                assert_eq!(status.code(), Some(1), "{listener}");
                break;
            }
            assert!(
                Instant::now() < deadline,
                "service stayed alive after {listener} failed"
            );
            std::thread::sleep(Duration::from_millis(20));
        }
    }
}
#[tokio::test]
async fn process_readiness_and_restart_with_synthetic_database() {
    let root = tempfile::tempdir().unwrap();
    let http_addr = std::net::TcpListener::bind("127.0.0.1:0")
        .unwrap()
        .local_addr()
        .unwrap();
    let mut cfg = config();
    cfg["http_listen_addr"] = http_addr.to_string().into();
    let client = reqwest::Client::builder()
        .no_proxy()
        .timeout(Duration::from_millis(250))
        .build()
        .unwrap();
    for _ in 0..2 {
        let mut process = start(&cfg, root.path());
        let deadline = Instant::now() + Duration::from_secs(5);
        loop {
            assert!(
                process.0.try_wait().unwrap().is_none(),
                "server exited before readiness"
            );
            if let Ok(response) = client
                .get(format!("http://{http_addr}/api/v1/ready"))
                .send()
                .await
            {
                if response.status().is_success() {
                    let body: serde_json::Value = response.json().await.unwrap();
                    assert_eq!(body["ready"], true);
                    assert_eq!(
                        body["protocol_version"],
                        dxx_matchmaking::protocol::CURRENT_PROTOCOL
                    );
                    assert_eq!(body["rooms"], 0);
                    assert_eq!(body["relay_sessions"], 0);
                    break;
                }
            }
            assert!(Instant::now() < deadline, "readiness deadline exceeded");
            tokio::time::sleep(Duration::from_millis(20)).await;
        }
        drop(process);
        assert!(client
            .get(format!("http://{http_addr}/api/v1/ready"))
            .send()
            .await
            .is_err());
    }
}

#[test]
fn process_rejects_partial_tls_configuration_before_listening() {
    for field in ["tls_cert_path", "tls_key_path"] {
        let root = tempfile::tempdir().unwrap();
        let mut cfg = config();
        cfg[field] = "synthetic-missing.pem".into();
        let mut process = start(&cfg, root.path());
        let deadline = Instant::now() + Duration::from_secs(5);
        loop {
            if let Some(status) = process.0.try_wait().unwrap() {
                assert_eq!(status.code(), Some(1));
                break;
            }
            assert!(
                Instant::now() < deadline,
                "partial TLS configuration stayed alive"
            );
            std::thread::sleep(Duration::from_millis(20));
        }
    }
}
