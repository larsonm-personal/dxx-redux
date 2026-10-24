# Public server: clone and run

Use an Ubuntu 22.04+ or Debian 12+ VPS with systemd and a checkout at a path without spaces. Build as a non-root SSH user with sudo, or deploy a prebuilt Linux binary as root using `--binary /absolute/path`. Point a DNS hostname directly at the VPS (DNS-only, not a CDN proxy); any AAAA record must also work. In the provider firewall allow TCP 80/443 and UDP 3478/3479/9001, keeping SSH access. Do not expose TCP 18080, 18081 or 9000.

From the cloned repository:

```sh
bash server/install.sh
```

It asks for your hostname, certificate contact email, Google **Web** OAuth client ID, and agreement to Let's Encrypt terms. Use the same client ID as the Android build, not an Android client ID or client secret. No Google account login on the VPS is needed.

The script installs build prerequisites, builds this checkout with the Rust version in `rust_version.txt` and locked dependencies, obtains/reuses a certificate, installs renewal and nginx, starts the daemon, and verifies local HTTPS readiness and the login-challenge route. It adds the required ports if UFW is already active; it never enables/resets a firewall or edits provider firewall rules. Other host firewalls must be configured separately.

## Reruns and updates

Run the same command to repair or update. Saved choices in `/etc/dxx-matchmaking/environment` are reused. Update your Git checkout explicitly first when desired; the installer never pulls or changes branches. The daemon runs as non-login `dxxmatch`, with root-owned binaries in `/opt/dxx-matchmaking/bin`, configuration in `/etc/dxx-matchmaking/server_config.jsonc`, data in `/var/lib/dxx-matchmaking`, and logs in `/var/log/dxx-matchmaking`. Existing files there are preserved. Legacy checkout-based services fail closed for explicit config/database migration rather than silently creating a replacement database. Existing systemd drop-ins must be reviewed/removed first, because they could override security settings.

To change saved choices, pass `--domain HOST --email EMAIL --google-client-id ID`. An existing Certbot account can be reused with `--acme-account ID` instead of an email; both apt and Snap renewal timers are supported. For unattended first installation also pass `--accept-acme-terms`. `deploy_build.sh` and `deploy_service.sh` forward to this complete public-install workflow; self-signed LAN experiments remain separate and are not accepted for production Google sign-in.

The managed environment overrides transport/auth settings: public HTTPS origin, Google audience, loopback TCP listeners (9000 WebSocket, 18080 HTTP, 18081 admin), UDP endpoints, trusted local proxy, no native TLS, production authentication, and logging. Other configuration (database, bans, quotas, MOTD) is preserved. Admin endpoints are accessible only locally, e.g. through SSH tunneling, and still require an admin token configured by the operator. The service cannot write its executable or read home directories; defaults cap it at 384 MB RAM and half a CPU core. Review those limits with load-test evidence before increasing them.

The nginx site preserves `/var/www/dxx-landing/index.html` as the landing page and uses a separate `/var/www/dxx-acme` certificate challenge root. Matchmaking routes are `/ws` and `/api/`; no other website's nginx configuration is rewritten. See `landing/README.md` to populate the landing page. The installer does not install compilers or update packages for prebuilt deployment when the runtime tools already exist.

On activation failure, managed files are restored and the previous daemon restarted if it was running. Snapshots remain under `/var/backups/dxx-matchmaking/`; no database rollback is attempted. Installed packages, issued certificates, created directories and added firewall rules remain. An unchanged rerun does not reissue an unexpired certificate or rewrite unchanged file contents, but does restart the daemon to repair runtime state (disconnecting players). Schedule reruns accordingly.

Check `systemctl status dxx-matchmaking`, `journalctl -u dxx-matchmaking`, and `systemctl list-timers '*certbot*'`. Certificate renewal reloads nginx automatically. The public client URL is `wss://HOST/ws`. Local checks do not establish external firewall/UDP reachability or real Google account acceptance; finish with a device test. A failed DNS/ACME check must be fixed and rerun, not bypassed with insecure TLS.

Local orchestration regression tests (no sudo/network/system changes): `bash server/tests/test_install.sh`.
