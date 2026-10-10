# Server configuration continuation, 2026-10-09

GQ2-CHUNK-0069 diagnosis complete. All frozen rename-aware9/6/14 hunks/removals, current configurations, current template delta and actual parser/consumer scopes reviewed. No implementation or fresh runtime acceptance.

## Existing-owner continuation

- GQR-0204/GQF-0219: make shipped production/LAN examples and actual deployment updates valid for the actual Rust JSONC loader. Current LAN trailing comma is rejected by strict serde_json after comment stripping. Parse full shipped documents and actual generated output; preserve unknown/unrelated values and exact no-op bytes, avoid duplicate keys, validate before transactional publication
- BR-0107/GQF-0014: reject explicitly requested unreadable/malformed configuration and invalid overrides before state/database/listeners. Distinguish intentional fileless development with safe bindings; do not silently select public defaults
- BR-0106: explicitly select enabled authentication methods, require complete Google credentials when enabled and reject unverified GPGS tokens. Keep development bypass explicit and conspicuous with safe mode restrictions; preserve legitimate keypair-only mode
- BR-0108: validate both-or-neither TLS paths, fail partial/invalid pairs before listening and derive logs from validated mode. Keep intentional reverse-proxy configuration distinct
- GQR-0222: preserve Rust comment token boundaries without expanding JSON language; retain completed PowerShell repair and current quoted string/line-number controls
- GQR-0153 remains DONE: retain complete schema/template/type/duplicate and isolated default/file/env precedence acceptance. These fixtures do not prove full LAN example admission or startup/auth/TLS behavior

## Proposed acceptance when implementation authorized

Actual full shipped examples must parse through production loader and preserve intended listener/TLS/auth/PoW/limit settings. Actual deployment-to-loader controls cover existing quoted fields, escaped values, rerun/no-op, failed update prior bytes and published valid document. Startup tests must assert failed required configuration/auth/TLS exits before database or listeners. Coordinate existing owners and keep malformed/security/resource probes explicitly deferred until authorized. No engine format or original-file edits; no new finding or status change.

Terminal report SHA25647e84bd82ca005480bdad68921ffd3e1a0f4826da9befc31977eb946440994fa; scope fingerprintd4c3bd8473b4ff167443531ec18d259ae47314722289a29e94e1b8349ac7914a.
