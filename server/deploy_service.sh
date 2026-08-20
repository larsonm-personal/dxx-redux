#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd -- "$(dirname -- "$(readlink -f -- "$0")")" && pwd)"
BINARY="$SCRIPT_DIR/target/release/dxx-matchmaking"
SERVICE_NAME="dxx-matchmaking"
SERVICE_FILE="/etc/systemd/system/${SERVICE_NAME}.service"
RUN_USER="$(whoami)"

if [ ! -f "$BINARY" ]; then
    echo "ERROR: binary not found at $BINARY"
    echo "Run deploy_build.sh first"
    exit 1
fi

echo "Installing systemd service: $SERVICE_NAME"
echo "  Binary:           $BINARY"
echo "  Working directory: $SCRIPT_DIR"
echo "  User:             $RUN_USER"
echo ""

# Generate the unit file
cat <<EOF | sudo tee "$SERVICE_FILE" > /dev/null
[Unit]
Description=DXX-Redux Matchmaking Server
After=network.target

[Service]
Type=simple
User=${RUN_USER}
WorkingDirectory=${SCRIPT_DIR}
ExecStart=${BINARY}
Restart=on-failure
RestartSec=5

# Logging: stdout/stderr go to journald; file logging is configured in server_config.jsonc
StandardOutput=journal
StandardError=journal
SyslogIdentifier=${SERVICE_NAME}

# Hardening
NoNewPrivileges=true
ProtectSystem=strict
ProtectHome=read-only
ReadWritePaths=${SCRIPT_DIR} /var/log/dxx-matchmaking
PrivateTmp=true

# Allow binding to low ports (STUN uses 3478/3479) without running as root
AmbientCapabilities=CAP_NET_BIND_SERVICE

[Install]
WantedBy=multi-user.target
EOF

echo "Service file written to $SERVICE_FILE"

# Reload, enable, and start
sudo systemctl daemon-reload
sudo systemctl enable "$SERVICE_NAME"
sudo systemctl restart "$SERVICE_NAME"

echo ""
echo "Service status:"
sudo systemctl status "$SERVICE_NAME" --no-pager || true
echo ""
echo "View logs: journalctl -u $SERVICE_NAME -f"
