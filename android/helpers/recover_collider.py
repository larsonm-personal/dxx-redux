"""Recover the two Small Pardon Collider ordinals from the October 2026 publisher release."""

import argparse
import hashlib
from pathlib import Path

from recovered_mission_zip import write_mission_zip

DESCRIPTOR_SHA256 = "0676686e6ccc449dce8970c11cc5040ed93130ab181edfa81332ec132d324857"
HOG_SHA256 = "6793a2c3e3a813bbf7ec2fb9ab4cfed45657ea57526b28dd4394ad52b7ef591a"


def recover(descriptor, output):
    mission = descriptor.read_bytes()
    if hashlib.sha256(mission).hexdigest() != DESCRIPTOR_SHA256:
        raise ValueError("Descriptor differs from the verified Small Pardon Collider release")
    hog = descriptor.with_name("SPC-XL.hog").read_bytes()
    if hashlib.sha256(hog).hexdigest() != HOG_SHA256:
        raise ValueError("Sibling HOG differs from the verified Small Pardon Collider release")

    corrected = mission.replace(b"\r\nsp-xxl.rl2\r\n", b"\r\nspc-xxl.rl2\r\n")
    write_mission_zip(output, (("SPC-XL.mn2", corrected), ("SPC-XL.hog", hog)))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("descriptor", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    try:
        recover(args.descriptor, args.output)
    except (OSError, ValueError) as exc:
        parser.exit(1, f"{exc}\n")
    print(f"Created {args.output} with the original Small Pardon Collider HOG")
