"""Prepare the disposable android-dual CI emulators for a shared Wi-Fi LAN."""

import argparse
import ipaddress
import json
import os
from pathlib import Path
import re
import subprocess
import time


def prepare(serials, output):
    receipt = {"passed": False, "commands": [], "addresses": {}}

    def adb(serial, *args, allow_failure=False):
        command = ["adb", "-s", serial, *args]
        entry = {"serial": serial, "arguments": list(args)}
        receipt["commands"].append(entry)
        try:
            result = subprocess.run(command, capture_output=True, text=True, timeout=10)
            entry.update(returncode=result.returncode, stdout=result.stdout, stderr=result.stderr)
        except subprocess.TimeoutExpired as error:
            entry["error"] = "ADB command timed out after 10 seconds"
            raise RuntimeError(entry["error"]) from error
        if result.returncode and not allow_failure:
            raise RuntimeError(f"ADB command failed on {serial}: {args}")
        return result.stdout.strip() if result.returncode == 0 else ""

    def ethernet_flags(serial):
        link = adb(serial, "shell", "ip", "-o", "link", "show", "dev", "eth0")
        matches = re.findall(r"(?m)^\d+: eth0(?:@[^: ]+)?: <([^>]*)>", link)
        if len(matches) != 1:
            raise RuntimeError(f"Could not verify eth0 link state on {serial}")
        return matches[0].split(",")

    def snapshot(serial):
        for args in (
            ("ip", "addr"),
            ("ip", "rule"),
            ("ip", "-4", "route", "show", "table", "all"),
            ("ip", "neigh"),
            ("cmd", "wifi", "status"),
        ):
            adb(serial, "shell", *args)

    try:
        if os.environ.get("GITHUB_ACTIONS") != "true" or os.environ.get("SWEEP_LANE") != "android-dual":
            raise RuntimeError("Wi-Fi topology setup is restricted to the disposable android-dual CI lane")
        if (
            len(serials) not in (2, 3)
            or len(set(serials)) != len(serials)
            or any(not re.fullmatch(r"emulator-\d+", serial) for serial in serials)
        ):
            raise RuntimeError("Expected two or three distinct emulator serials")
        # Validate ALL targets before the first mutation
        for serial in serials:
            if adb(serial, "get-state") != "device" or adb(serial, "shell", "getprop", "ro.kernel.qemu") != "1":
                raise RuntimeError(f"Unverified emulator: {serial}")
            if adb(serial, "shell", "su", "0", "id", "-u") != "0":
                raise RuntimeError(f"Emulator root shell unavailable: {serial}")
            # An unaddressed Ethernet NIC still needs verification and disabling
            ethernet_flags(serial)
            for interface in ("wlan0", "eth0"):
                text = adb(serial, "shell", "ip", "-4", "addr", "show", "dev", interface)
                if interface == "eth0" and not text:
                    continue
                addresses = re.findall(r"\binet (\S+)", text)
                if len(addresses) != 1:
                    raise RuntimeError(f"Expected one IPv4 address on {serial}/{interface}")
                configured = ipaddress.IPv4Interface(addresses[0])
                address = str(configured.ip)
                # API 34 private Ethernet uses /8; shared Wi-Fi remains /24
                if interface == "eth0" and (address != "10.0.2.15" or configured.network.prefixlen not in (8, 24)):
                    raise RuntimeError(f"Unexpected private Ethernet address on {serial}")
                if interface == "wlan0":
                    if configured.network.prefixlen != 24:
                        raise RuntimeError(f"Expected /24 shared Wi-Fi address on {serial}")
                    if not 16 <= int(ipaddress.IPv4Address(address)) - int(ipaddress.IPv4Address("10.0.2.0")) <= 254:
                        raise RuntimeError(f"Unexpected shared Wi-Fi address on {serial}")
                    receipt["addresses"][serial] = address
            snapshot(serial)
        if len(set(receipt["addresses"].values())) != len(serials):
            raise RuntimeError("Shared Wi-Fi addresses must be distinct")
        for serial in serials:
            for peer, address in receipt["addresses"].items():
                if peer != serial:
                    adb(serial, "shell", "ip", "route", "get", address)
        # Remove only the competing private NIC, before any LAN test starts
        for serial in serials:
            adb(serial, "shell", "su", "0", "ip", "link", "set", "dev", "eth0", "down")
        deadline = time.monotonic() + 30
        while True:
            ready = True
            for serial in serials:
                if "UP" in ethernet_flags(serial):
                    ready = False
                for peer, address in receipt["addresses"].items():
                    if peer == serial:
                        continue
                    route = adb(serial, "shell", "ip", "route", "get", address, allow_failure=True)
                    source = re.escape(receipt["addresses"][serial])
                    if not re.search(r"\bdev wlan0\b", route) or not re.search(rf"\bsrc {source}(?:\s|$)", route):
                        ready = False
            if ready:
                break
            if time.monotonic() >= deadline:
                raise RuntimeError("Unbound peer routes did not converge to shared Wi-Fi")
            time.sleep(1)
        for serial in serials:
            snapshot(serial)
        receipt["passed"] = True
    except Exception as error:
        receipt["error"] = str(error)
        raise
    finally:
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--serials", nargs="+", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    prepare(args.serials, args.output)
