#!/usr/bin/env bash
# Generate a self-signed TLS certificate for LAN testing.
# Usage: ./generate_lan_cert.sh [IP_ADDRESS]
#   IP_ADDRESS defaults to the first non-loopback IPv4 address found.
set -e

SCRIPT_DIR="$(cd -- "$(dirname -- "$(readlink -f -- "$0")")" && pwd)"
CERT_DIR="$SCRIPT_DIR/lan_certs"

# Determine LAN IP
if [ -n "$1" ]; then
    LAN_IP="$1"
else
    LAN_IP=$(hostname -I 2>/dev/null | awk '{print $1}')
    if [ -z "$LAN_IP" ]; then
        LAN_IP=$(ip -4 addr show scope global | grep -oP '(?<=inet\s)\d+(\.\d+){3}' | head -1)
    fi
    if [ -z "$LAN_IP" ]; then
        echo "ERROR: could not detect LAN IP. Pass it as argument: $0 192.168.1.5"
        exit 1
    fi
fi

echo "Generating self-signed certificate for: $LAN_IP"
mkdir -p "$CERT_DIR"

CERT_FILE="$CERT_DIR/cert.pem"
KEY_FILE="$CERT_DIR/key.pem"

openssl req -x509 -newkey rsa:2048 -nodes \
    -keyout "$KEY_FILE" \
    -out "$CERT_FILE" \
    -days 365 \
    -subj "/CN=dxx-matchmaking-lan" \
    -addext "subjectAltName=IP:$LAN_IP,IP:127.0.0.1"

echo ""
echo "Certificate generated:"
echo "  Cert: $CERT_FILE"
echo "  Key:  $KEY_FILE"
echo "  IP:   $LAN_IP"
echo ""
echo "To use, set in server_config.jsonc:"
echo "  tls_cert_path: \"$CERT_FILE\","
echo "  tls_key_path: \"$KEY_FILE\","
echo "  ws_listen_addr: \"0.0.0.0:9000\","
echo ""
echo "Clients connect via: wss://$LAN_IP:9000/ws"
