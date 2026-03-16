use std::io::BufReader;
use std::sync::Arc;

use rustls::pki_types::{CertificateDer, PrivateKeyDer};

/// Load TLS certificate chain and private key from PEM files.
/// Returns `Ok(None)` if either path is empty (TLS not configured).
pub fn load_rustls_config(
    cert_path: &str,
    key_path: &str,
) -> Result<Option<Arc<rustls::ServerConfig>>, Box<dyn std::error::Error>> {
    if cert_path.is_empty() || key_path.is_empty() {
        return Ok(None);
    }

    let cert_data = std::fs::read(cert_path)
        .map_err(|e| format!("failed to read TLS cert file '{cert_path}': {e}"))?;
    let certs: Vec<CertificateDer<'static>> =
        rustls_pemfile::certs(&mut BufReader::new(&cert_data[..]))
            .collect::<Result<Vec<_>, _>>()
            .map_err(|e| format!("failed to parse TLS cert PEM: {e}"))?;
    if certs.is_empty() {
        return Err(format!("no certificates found in '{cert_path}'").into());
    }

    let key_data = std::fs::read(key_path)
        .map_err(|e| format!("failed to read TLS key file '{key_path}': {e}"))?;
    let key: PrivateKeyDer<'static> =
        rustls_pemfile::private_key(&mut BufReader::new(&key_data[..]))
            .map_err(|e| format!("failed to parse TLS key PEM: {e}"))?
            .ok_or_else(|| format!("no private key found in '{key_path}'"))?;

    let config = rustls::ServerConfig::builder()
        .with_no_client_auth()
        .with_single_cert(certs, key)
        .map_err(|e| format!("failed to build TLS config: {e}"))?;

    Ok(Some(Arc::new(config)))
}
