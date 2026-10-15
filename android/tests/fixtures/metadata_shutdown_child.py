"""Harmless EOF-driven child shared by Python and PowerShell shutdown tests."""
import sys
import time

mode = sys.argv[1]
sys.stdin.read()
if mode == "slow":
    time.sleep(2.2)
print("shutdown stdout", flush=True)
print("shutdown diagnostic", file=sys.stderr, flush=True)
if mode == "flood":
    print("O" * 262144, flush=True)
    print("E" * 262144, file=sys.stderr, flush=True)
if mode == "timeout":
    time.sleep(60)
sys.exit(23 if mode == "nonzero" else 0)
