//! Bounded ownership for Google login challenges and reconnect credentials

use std::{
    collections::HashMap,
    net::IpAddr,
    sync::{Arc, Mutex},
    time::{Duration, Instant},
};

use ring::rand::{SecureRandom, SystemRandom};
use serde::Serialize;
use sha2::{Digest, Sha256};

use crate::google_identity::{GoogleVerifier, VerifyError};

const CHALLENGE_LIFETIME: Duration = Duration::from_secs(300);
const SESSION_LIFETIME: Duration = Duration::from_secs(12 * 60 * 60);
const MAX_CHALLENGES: usize = 1024;
const MAX_SOURCE_CHALLENGES: usize = 16;
const MAX_SESSIONS: usize = 2048;
const MAX_ACCOUNT_SESSIONS: usize = 8;

#[derive(Debug, PartialEq, Eq)]
pub enum LoginError {
    InvalidRequest,
    InvalidCredential,
    Capacity,
    Unavailable,
}

#[derive(Serialize)]
pub struct LoginChallenge {
    pub challenge: String,
    pub expires_in_seconds: u64,
}

#[derive(Serialize)]
pub struct LoginCredential {
    pub session_token: String,
    pub expires_in_seconds: u64,
}

#[derive(Clone)]
pub struct CredentialIdentity {
    pub subject: String,
    pub expires: Instant,
}

/// Non-secret handle used to enforce revocation on an established connection
#[derive(Clone, Copy)]
pub struct CredentialHandle([u8; 32]);

struct PendingChallenge {
    source: IpAddr,
    expires: Instant,
    used: bool,
    issued: Option<[u8; 32]>,
}

struct LoginState {
    challenges: HashMap<String, PendingChallenge>,
    // Store only hashes of bearer credentials, never the returned secrets
    sessions: HashMap<[u8; 32], CredentialIdentity>,
    next_cleanup: Instant,
}

impl LoginState {
    fn prune(&mut self, now: Instant) {
        if now >= self.next_cleanup {
            self.challenges.retain(|_, item| now < item.expires);
            self.sessions.retain(|_, item| now < item.expires);
            self.next_cleanup = now + Duration::from_secs(1);
        }
    }
}

pub struct GoogleLogin {
    origin: String,
    verifier: Arc<GoogleVerifier>,
    state: Mutex<LoginState>,
}

impl GoogleLogin {
    pub fn new(origin: &str, audience: &str) -> Result<Self, &'static str> {
        Self::with_verifier(origin, Arc::new(GoogleVerifier::new(audience)?))
    }

    pub(crate) fn with_verifier(
        origin: &str,
        verifier: Arc<GoogleVerifier>,
    ) -> Result<Self, &'static str> {
        let origin =
            canonical_origin(origin).ok_or("Invalid public Google authentication origin")?;
        Ok(Self {
            origin,
            verifier,
            state: Mutex::new(LoginState {
                challenges: HashMap::new(),
                sessions: HashMap::new(),
                next_cleanup: Instant::now(),
            }),
        })
    }

    /// The HTTP boundary separately applies source and global request/work admission
    pub fn challenge(&self, source: IpAddr, now: Instant) -> Result<LoginChallenge, LoginError> {
        let mut state = self.state.lock().unwrap();
        state.prune(now);
        if state.challenges.len() >= MAX_CHALLENGES
            || state
                .challenges
                .values()
                .filter(|entry| entry.source == source)
                .count()
                >= MAX_SOURCE_CHALLENGES
        {
            return Err(LoginError::Capacity);
        }
        let challenge = random_secret()?;
        if state.challenges.contains_key(&challenge) {
            return Err(LoginError::Unavailable);
        }
        state.challenges.insert(
            challenge.clone(),
            PendingChallenge {
                source,
                expires: now + CHALLENGE_LIFETIME,
                used: false,
                issued: None,
            },
        );
        Ok(LoginChallenge {
            challenge,
            expires_in_seconds: CHALLENGE_LIFETIME.as_secs(),
        })
    }

    /// Mark used before awaiting verification, so concurrent/replayed exchanges cannot win twice
    /// The source may change during the interactive flow; the random challenge is its capability
    pub async fn exchange(
        &self,
        challenge: &str,
        token: &str,
        deadline: tokio::time::Instant,
    ) -> Result<LoginCredential, LoginError> {
        if !valid_secret(challenge) {
            return Err(LoginError::InvalidRequest);
        }
        {
            let mut state = self.state.lock().unwrap();
            let pending = state
                .challenges
                .get_mut(challenge)
                .ok_or(LoginError::InvalidCredential)?;
            if pending.used || Instant::now() >= pending.expires {
                return Err(LoginError::InvalidCredential);
            }
            pending.used = true;
        }
        let nonce = login_nonce(&self.origin, challenge).ok_or(LoginError::InvalidRequest)?;
        let identity = self
            .verifier
            .verify(token, &nonce, deadline)
            .await
            .map_err(|error| match error {
                VerifyError::InvalidToken | VerifyError::InvalidClaims => {
                    LoginError::InvalidCredential
                }
                VerifyError::KeysUnavailable | VerifyError::Deadline => LoginError::Unavailable,
            })?;
        let now = Instant::now();
        let mut state = self.state.lock().unwrap();
        state.prune(now);
        // Cancellation can remove the capability while verification is in flight
        if !state
            .challenges
            .get(challenge)
            .is_some_and(|entry| now < entry.expires)
        {
            return Err(LoginError::InvalidCredential);
        }
        if state.sessions.len() >= MAX_SESSIONS
            || state
                .sessions
                .values()
                .filter(|entry| entry.subject == identity.subject())
                .count()
                >= MAX_ACCOUNT_SESSIONS
        {
            return Err(LoginError::Capacity);
        }
        let token = random_secret()?;
        let key = credential_hash(&token).ok_or(LoginError::Unavailable)?;
        if state.sessions.contains_key(&key) {
            return Err(LoginError::Unavailable);
        }
        state.sessions.insert(
            key,
            CredentialIdentity {
                subject: identity.subject().into(),
                expires: now + SESSION_LIFETIME,
            },
        );
        // Keep a bounded receipt so the initiator can revoke a lost exchange response
        let receipt = state.challenges.get_mut(challenge).unwrap();
        receipt.issued = Some(key);
        receipt.expires = now + CHALLENGE_LIFETIME;
        Ok(LoginCredential {
            session_token: token,
            expires_in_seconds: SESSION_LIFETIME.as_secs(),
        })
    }

    /// Possession-only cancellation; unknown capabilities never allocate state
    pub fn cancel(&self, challenge: &str) {
        if !valid_secret(challenge) {
            return;
        }
        let mut state = self.state.lock().unwrap();
        if let Some(receipt) = state.challenges.remove(challenge) {
            if let Some(key) = receipt.issued {
                state.sessions.remove(&key);
            }
        }
    }

    /// Lookup never extends lifetime; the WebSocket boundary must also enforce bans/expiry
    pub fn credential(&self, token: &str, now: Instant) -> Option<CredentialIdentity> {
        let key = credential_hash(token)?;
        let state = self.state.lock().unwrap();
        state
            .sessions
            .get(&key)
            .filter(|identity| now < identity.expires)
            .cloned()
    }

    pub fn revoke(&self, token: &str) {
        if let Some(key) = credential_hash(token) {
            self.state.lock().unwrap().sessions.remove(&key);
        }
    }

    #[cfg(test)]
    pub(crate) fn expire_fixture_credential(&self, token: &str) {
        let key = credential_hash(token).unwrap();
        self.state
            .lock()
            .unwrap()
            .sessions
            .get_mut(&key)
            .unwrap()
            .expires = Instant::now();
    }

    pub fn handle(&self, token: &str) -> Option<CredentialHandle> {
        credential_hash(token).map(CredentialHandle)
    }

    pub fn is_active(&self, handle: CredentialHandle, now: Instant) -> bool {
        self.state
            .lock()
            .unwrap()
            .sessions
            .get(&handle.0)
            .is_some_and(|identity| now < identity.expires)
    }
}

/// Shared with the Android Google login flow: HTTPS origin, explicit port, no path
pub fn canonical_origin(value: &str) -> Option<String> {
    if value.len() > 512 {
        return None;
    }
    let url = reqwest::Url::parse(value).ok()?;
    if url.scheme() != "https"
        || !url.username().is_empty()
        || url.password().is_some()
        || url.path() != "/"
        || url.query().is_some()
        || url.fragment().is_some()
    {
        return None;
    }
    let port = url.port_or_known_default()?;
    if port == 0 {
        return None;
    }
    Some(format!("https://{}:{port}", url.host()?))
}

/// Shared wire rule: hex SHA-256 of UTF-8 domain separator, canonical origin and challenge
/// Clients compute from their selected server, never from a server-provided origin/nonce
pub fn login_nonce(origin: &str, challenge: &str) -> Option<String> {
    if !valid_secret(challenge) {
        return None;
    }
    let origin = canonical_origin(origin)?;
    Some(hex::encode(Sha256::digest(
        format!("dxx-google-login-v1\n{origin}\n{challenge}").as_bytes(),
    )))
}

fn random_secret() -> Result<String, LoginError> {
    let mut bytes = [0u8; 32];
    SystemRandom::new()
        .fill(&mut bytes)
        .map_err(|_| LoginError::Unavailable)?;
    Ok(hex::encode(bytes))
}

fn valid_secret(value: &str) -> bool {
    value.len() == 64
        && value
            .bytes()
            .all(|b| b.is_ascii_digit() || (b'a'..=b'f').contains(&b))
}

fn credential_hash(value: &str) -> Option<[u8; 32]> {
    valid_secret(value).then(|| Sha256::digest(value.as_bytes()).into())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn nonce_is_recipient_bound_and_origin_is_canonical() {
        let challenge = "a".repeat(64);
        assert_eq!(
            canonical_origin("https://MATCH.example/"),
            Some("https://match.example:443".into())
        );
        assert_eq!(
            canonical_origin("https://[::1]:444"),
            Some("https://[::1]:444".into())
        );
        let nonce = login_nonce("https://match.example", &challenge).unwrap();
        assert_eq!(
            nonce,
            login_nonce("https://match.example:443/", &challenge).unwrap()
        );
        assert_ne!(
            nonce,
            login_nonce("https://other.example", &challenge).unwrap()
        );
        assert_ne!(
            nonce,
            login_nonce("https://match.example:444", &challenge).unwrap()
        );
        for invalid in [
            "http://match.example",
            "https://user@match.example",
            "https://match.example/ws",
            "https://match.example?query",
            "https://match.example#fragment",
        ] {
            assert!(canonical_origin(invalid).is_none());
        }
        assert!(login_nonce("https://match.example", "short").is_none());
    }

    #[test]
    fn challenges_have_finite_source_global_capacity_and_lifetime() {
        let login = GoogleLogin::new("https://match.example", "fixture-audience").unwrap();
        let now = Instant::now();
        let ip: IpAddr = "127.0.0.1".parse().unwrap();
        for _ in 0..MAX_SOURCE_CHALLENGES {
            login.challenge(ip, now).unwrap();
        }
        assert!(matches!(
            login.challenge(ip, now),
            Err(LoginError::Capacity)
        ));
        for index in 1..MAX_CHALLENGES / MAX_SOURCE_CHALLENGES {
            let ip = IpAddr::V4(std::net::Ipv4Addr::new(10, 0, 0, index as u8));
            for _ in 0..MAX_SOURCE_CHALLENGES {
                login.challenge(ip, now).unwrap();
            }
        }
        assert!(matches!(
            login.challenge("192.0.2.1".parse().unwrap(), now),
            Err(LoginError::Capacity)
        ));
        assert_eq!(login.state.lock().unwrap().challenges.len(), MAX_CHALLENGES);
        assert!(login.challenge(ip, now + CHALLENGE_LIFETIME).is_ok());
        assert_eq!(login.state.lock().unwrap().challenges.len(), 1);
    }
}
