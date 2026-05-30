# Server Deployment Scripts Plan

## Goal
Create two scripts for deploying the dxx-matchmaking server on a Linux server with Let's Encrypt.

## Script 1: `server/deploy_build.sh`
All-in-one build script:
1. `git pull` to update the repo
2. Install or upgrade Rust toolchain:
   - If `rustup` not found: install from rustup.rs (`-y` for non-interactive)
   - If available: run `rustup update stable`
3. `cargo build --release` in server/
4. Copy `config.json5.default` to `server_config.json5` if it doesn't exist; if it does, prompt to overwrite or keep
5. Call script 2 to install/update the systemd service

## Script 2: `server/deploy_service.sh`
Systemd service management:
1. Install a systemd unit file `dxx-matchmaking.service` to `/etc/systemd/system/`
2. Service configuration:
   - `WorkingDirectory` = the server/ dir (so relative db_path works)
   - `ExecStart` = path to release binary
   - Config file: `server_config.json5` in the server/ dir (the binary reads it by default from cwd)
   - Logging: `log_dir` in config points to `/var/log/dxx-matchmaking/`; also stdout/stderr go to journald
   - `Restart=on-failure`
   - `After=network.target`
   - Run as current user (not root) via `User=` directive
3. `systemctl daemon-reload`
4. `systemctl enable dxx-matchmaking` (start on boot)
5. `systemctl restart dxx-matchmaking` (start immediately)

## Config locations
- Config file: `server/server_config.json5` (cwd-relative, where the binary expects it)
- Database: `server/dxx_matchmaking.db` (default cwd-relative path)
- Logs: `/var/log/dxx-matchmaking/` (daily rotation via tracing-appender) + journald
- TLS: Let's Encrypt certs, typically `/etc/letsencrypt/live/<domain>/fullchain.pem` and `privkey.pem`

## Default config (config.json5.default)
A production-ready template with:
- All defaults from the template
- `log_dir` set to `/var/log/dxx-matchmaking`
- TLS paths commented with Let's Encrypt example paths
- Admin token placeholder
