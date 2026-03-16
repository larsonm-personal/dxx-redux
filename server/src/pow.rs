use ed25519_dalek::{Signature, VerifyingKey};
use sha2::{Digest, Sha256};
use tracing::debug;

/// Generate a random hex challenge string (32 bytes = 64 hex chars).
pub fn generate_challenge() -> String {
    let bytes: [u8; 32] = rand::random();
    hex::encode(bytes)
}

/// Verify a proof-of-work solution.
/// Returns true if SHA256(challenge || solution) has at least `difficulty`
/// leading zero bits.
pub fn verify_pow(challenge: &str, solution: &str, difficulty: u8) -> bool {
    let mut hasher = Sha256::new();
    hasher.update(challenge.as_bytes());
    hasher.update(solution.as_bytes());
    let hash = hasher.finalize();
    leading_zero_bits(&hash) >= difficulty as u32
}

/// Count leading zero bits in a byte slice.
pub fn leading_zero_bits(data: &[u8]) -> u32 {
    let mut count = 0u32;
    for &byte in data {
        if byte == 0 {
            count += 8;
        } else {
            count += byte.leading_zeros();
            break;
        }
    }
    count
}

/// Verify an Ed25519 signature.
/// `pubkey_hex`: 32-byte public key as 64 hex chars.
/// `message`: the bytes that were signed.
/// `signature_hex`: 64-byte signature as 128 hex chars.
pub fn verify_ed25519_signature(pubkey_hex: &str, message: &[u8], signature_hex: &str) -> bool {
    let pubkey_bytes = match hex::decode(pubkey_hex) {
        Ok(b) if b.len() == 32 => b,
        _ => {
            debug!("invalid public key hex");
            return false;
        }
    };
    let sig_bytes = match hex::decode(signature_hex) {
        Ok(b) if b.len() == 64 => b,
        _ => {
            debug!("invalid signature hex");
            return false;
        }
    };

    let verifying_key = match VerifyingKey::from_bytes(pubkey_bytes.as_slice().try_into().unwrap())
    {
        Ok(k) => k,
        Err(_) => {
            debug!("invalid ed25519 public key");
            return false;
        }
    };
    let signature = Signature::from_bytes(sig_bytes.as_slice().try_into().unwrap());

    use ed25519_dalek::Verifier;
    verifying_key.verify(message, &signature).is_ok()
}

/// Hash a public key for DB storage (SHA-256 of the raw hex string).
pub fn hash_pubkey(pubkey_hex: &str) -> String {
    let mut hasher = Sha256::new();
    hasher.update(pubkey_hex.as_bytes());
    hex::encode(hasher.finalize())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_pow_verify() {
        // Difficulty 0 always passes
        assert!(verify_pow("test", "anything", 0));
    }

    #[test]
    fn test_pow_leading_zeros() {
        assert_eq!(leading_zero_bits(&[0x00, 0x00, 0xFF]), 16);
        assert_eq!(leading_zero_bits(&[0x0F, 0xFF]), 4);
        assert_eq!(leading_zero_bits(&[0xFF]), 0);
        assert_eq!(leading_zero_bits(&[0x01]), 7);
    }

    #[test]
    fn test_pow_brute_force_low_difficulty() {
        let challenge = generate_challenge();
        // Difficulty 8 = 1 byte of leading zeros, ~256 tries avg
        let mut found = false;
        for i in 0..100_000u64 {
            let solution = i.to_string();
            if verify_pow(&challenge, &solution, 8) {
                found = true;
                break;
            }
        }
        assert!(
            found,
            "should find a solution within 100k tries for difficulty 8"
        );
    }

    #[test]
    fn test_ed25519_roundtrip() {
        use ed25519_dalek::{Signer, SigningKey};

        let signing_key = SigningKey::generate(&mut rand::rng());
        let verifying_key = signing_key.verifying_key();
        let pubkey_hex = hex::encode(verifying_key.as_bytes());

        let message = b"test message for signing";
        let signature = signing_key.sign(message);
        let sig_hex = hex::encode(signature.to_bytes());

        assert!(verify_ed25519_signature(&pubkey_hex, message, &sig_hex));
        // Wrong message should fail
        assert!(!verify_ed25519_signature(
            &pubkey_hex,
            b"wrong message",
            &sig_hex
        ));
    }

    #[test]
    fn test_hash_pubkey() {
        let hash = hash_pubkey("abc123");
        assert_eq!(hash.len(), 64); // SHA-256 = 32 bytes = 64 hex chars
    }
}
