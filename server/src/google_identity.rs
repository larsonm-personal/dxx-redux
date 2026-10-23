//! Google account verification for matchmaking, independent of Play Games

use std::{collections::BTreeMap, sync::Mutex, time::Duration};

use jsonwebtoken::{decode, decode_header, Algorithm, DecodingKey, Validation};
use serde::Deserialize;
use tokio::time::Instant;

const GOOGLE_KEYS_URL: &str = "https://www.googleapis.com/oauth2/v3/certs";
const MAX_TOKEN_BYTES: usize = 8192;
const MAX_KEYS_BYTES: usize = 32 * 1024;
const MAX_KEYS: usize = 16;
const REFRESH_INTERVAL: Duration = Duration::from_secs(60);
const MAX_CACHE_SECONDS: u64 = 6 * 60 * 60;

#[derive(Debug, PartialEq, Eq)]
pub enum VerifyError {
    InvalidToken,
    InvalidClaims,
    KeysUnavailable,
    Deadline,
}

/// Only constructed after signature, claims and caller-supplied nonce verification
pub struct GoogleIdentity {
    subject: String,
    expires_at: u64,
}

impl GoogleIdentity {
    pub fn subject(&self) -> &str {
        &self.subject
    }

    pub fn expires_at(&self) -> u64 {
        self.expires_at
    }
}

#[derive(Deserialize)]
struct Claims {
    // This flow requests a token for exactly one configured OAuth audience
    #[serde(rename = "aud")]
    _audience: String,
    sub: String,
    nonce: String,
    iat: u64,
    exp: u64,
}

#[derive(Deserialize)]
struct GoogleKeys {
    keys: Vec<GoogleKey>,
}

#[derive(Deserialize)]
struct GoogleKey {
    kid: String,
    kty: String,
    alg: String,
    #[serde(rename = "use")]
    usage: String,
    n: String,
    e: String,
}

struct KeyCache {
    keys: BTreeMap<String, DecodingKey>,
    expires: Instant,
    next_refresh: Instant,
}

/// Share one instance per server; callers also retain global authentication admission
pub struct GoogleVerifier {
    client: reqwest::Client,
    keys_url: String,
    validation: Validation,
    cache: Mutex<KeyCache>,
    refresh: tokio::sync::Mutex<()>,
}

impl GoogleVerifier {
    pub fn new(audience: &str) -> Result<Self, &'static str> {
        Self::build(audience, GOOGLE_KEYS_URL)
    }

    // The endpoint is never selected by a credential or public server configuration
    fn build(audience: &str, keys_url: &str) -> Result<Self, &'static str> {
        if audience.is_empty()
            || audience.len() > 256
            || !audience.bytes().all(|b| b.is_ascii_graphic())
        {
            return Err("Invalid Google OAuth audience");
        }
        let client = reqwest::Client::builder()
            .timeout(Duration::from_secs(3))
            .connect_timeout(Duration::from_secs(2))
            .redirect(reqwest::redirect::Policy::none())
            .build()
            .map_err(|_| "Cannot initialize Google key client")?;
        let mut validation = Validation::new(Algorithm::RS256);
        validation.set_audience(&[audience]);
        validation.set_issuer(&["https://accounts.google.com", "accounts.google.com"]);
        validation.set_required_spec_claims(&["exp", "iss", "aud", "sub"]);
        validation.leeway = 0;
        validation.validate_nbf = true;
        let now = Instant::now();
        Ok(Self {
            client,
            keys_url: keys_url.into(),
            validation,
            cache: Mutex::new(KeyCache {
                keys: BTreeMap::new(),
                expires: now,
                next_refresh: now,
            }),
            refresh: tokio::sync::Mutex::new(()),
        })
    }

    /// The caller must own and consume the expected nonce, scoped to its login attempt
    /// Matching a nonce here alone does not establish single-use or recipient binding
    pub async fn verify(
        &self,
        token: &str,
        expected_nonce: &str,
        deadline: Instant,
    ) -> Result<GoogleIdentity, VerifyError> {
        if Instant::now() >= deadline {
            return Err(VerifyError::Deadline);
        }
        if token.is_empty()
            || token.len() > MAX_TOKEN_BYTES
            || !(32..=256).contains(&expected_nonce.len())
            || !expected_nonce.bytes().all(|b| b.is_ascii_graphic())
        {
            return Err(VerifyError::InvalidToken);
        }
        let header = decode_header(token).map_err(|_| VerifyError::InvalidToken)?;
        if header.alg != Algorithm::RS256
            || header.jku.is_some()
            || header.jwk.is_some()
            || header.x5u.is_some()
            || header.crit.is_some()
            || header.enc.is_some()
            || header.zip.is_some()
        {
            return Err(VerifyError::InvalidToken);
        }
        let kid = header
            .kid
            .filter(|kid| valid_key_id(kid))
            .ok_or(VerifyError::InvalidToken)?;
        let key = tokio::time::timeout_at(deadline, self.key(&kid))
            .await
            .map_err(|_| VerifyError::Deadline)??;
        let claims = decode::<Claims>(token, &key, &self.validation)
            .map_err(|_| VerifyError::InvalidToken)?
            .claims;
        let now = jsonwebtoken::get_current_timestamp();
        if claims.sub.is_empty()
            || claims.sub.len() > 255
            || !claims.sub.bytes().all(|b| b.is_ascii_graphic())
            || claims.nonce != expected_nonce
            || claims.iat > now.saturating_add(30)
            || claims.iat >= claims.exp
            || claims.exp <= now
        {
            return Err(VerifyError::InvalidClaims);
        }
        if Instant::now() >= deadline {
            return Err(VerifyError::Deadline);
        }
        Ok(GoogleIdentity {
            subject: claims.sub,
            expires_at: claims.exp,
        })
    }

    fn cached_key(&self, kid: &str) -> Option<DecodingKey> {
        let cache = self.cache.lock().unwrap();
        (Instant::now() < cache.expires)
            .then(|| cache.keys.get(kid).cloned())
            .flatten()
    }

    async fn key(&self, kid: &str) -> Result<DecodingKey, VerifyError> {
        if let Some(key) = self.cached_key(kid) {
            return Ok(key);
        }
        // Valid cached keys remain available while a refresh waits for the network
        let _refresh = self.refresh.lock().await;
        if let Some(key) = self.cached_key(kid) {
            return Ok(key);
        }
        {
            let mut cache = self.cache.lock().unwrap();
            let now = Instant::now();
            if now < cache.next_refresh {
                return Err(VerifyError::KeysUnavailable);
            }
            // Set before awaiting so cancellation and failed fetches also consume the budget
            cache.next_refresh = now + REFRESH_INTERVAL;
        }
        let (keys, lifetime) = self.fetch_keys().await?;
        {
            let mut cache = self.cache.lock().unwrap();
            cache.keys = keys;
            cache.expires = Instant::now() + lifetime;
        }
        self.cached_key(kid).ok_or(VerifyError::KeysUnavailable)
    }

    async fn fetch_keys(&self) -> Result<(BTreeMap<String, DecodingKey>, Duration), VerifyError> {
        let mut response = self
            .client
            .get(&self.keys_url)
            .send()
            .await
            .map_err(|_| VerifyError::KeysUnavailable)?;
        if !response.status().is_success() {
            return Err(VerifyError::KeysUnavailable);
        }
        let lifetime = cache_lifetime(response.headers());
        let mut body = Vec::new();
        while let Some(chunk) = response
            .chunk()
            .await
            .map_err(|_| VerifyError::KeysUnavailable)?
        {
            if chunk.len() > MAX_KEYS_BYTES.saturating_sub(body.len()) {
                return Err(VerifyError::KeysUnavailable);
            }
            body.extend_from_slice(&chunk);
        }
        let document: GoogleKeys =
            serde_json::from_slice(&body).map_err(|_| VerifyError::KeysUnavailable)?;
        if document.keys.is_empty() || document.keys.len() > MAX_KEYS {
            return Err(VerifyError::KeysUnavailable);
        }
        let mut keys = BTreeMap::new();
        for key in document.keys {
            if !valid_key_id(&key.kid)
                || key.kty != "RSA"
                || key.alg != "RS256"
                || key.usage != "sig"
                || !(342..=683).contains(&key.n.len())
                || key.e != "AQAB"
            {
                return Err(VerifyError::KeysUnavailable);
            }
            let decoded = DecodingKey::from_rsa_components(&key.n, &key.e)
                .map_err(|_| VerifyError::KeysUnavailable)?;
            if keys.insert(key.kid, decoded).is_some() {
                return Err(VerifyError::KeysUnavailable);
            }
        }
        Ok((keys, lifetime))
    }
}

fn valid_key_id(kid: &str) -> bool {
    !kid.is_empty()
        && kid.len() <= 128
        && kid
            .bytes()
            .all(|b| b.is_ascii_alphanumeric() || b == b'-' || b == b'_')
}

fn cache_lifetime(headers: &reqwest::header::HeaderMap) -> Duration {
    let mut max_age: Option<u64> = None;
    for value in headers.get_all(reqwest::header::CACHE_CONTROL) {
        let Ok(value) = value.to_str() else {
            return Duration::ZERO;
        };
        for directive in value.split(',').map(str::trim) {
            if directive.eq_ignore_ascii_case("no-store")
                || directive.eq_ignore_ascii_case("no-cache")
            {
                return Duration::ZERO;
            }
            if let Some((name, value)) = directive.split_once('=') {
                if name.trim().eq_ignore_ascii_case("max-age") {
                    let Ok(value) = value.trim().trim_matches('"').parse::<u64>() else {
                        return Duration::ZERO;
                    };
                    max_age = Some(max_age.map_or(value, |previous| previous.min(value)));
                }
            }
        }
    }
    let age = match headers.get(reqwest::header::AGE) {
        Some(value) => match value.to_str().ok().and_then(|v| v.parse::<u64>().ok()) {
            Some(age) => age,
            None => return Duration::ZERO,
        },
        None => 0,
    };
    Duration::from_secs(
        max_age
            .unwrap_or(300)
            .saturating_sub(age)
            .min(MAX_CACHE_SECONDS),
    )
}

#[cfg(test)]
#[path = "google_identity_tests.rs"]
mod tests;
