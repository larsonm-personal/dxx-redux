"""Build a playable Picasso ZIP from the publisher's extracted descriptor and sibling HOG.

Pass the path to Picasso.mn2 (picasso.mn2 in levelpack.7z) and a new ZIP path.
The new descriptor names the level stored in the original HOG, picaso.rl2.
"""

import argparse
import hashlib
from pathlib import Path

from recovered_mission_zip import write_mission_zip

DESCRIPTOR_SHA256 = "ebc27c891771fcc81ad22234d63a0e985ad94463cbf6e5f2465a327b7df4f836"
HOG_SHA256 = "9ef94392b06206cf7ce6ffa0dd8491df79eb16bd56db4978226b5da3db0e17c0"


def recover(descriptor, output):
    mission = descriptor.read_bytes()
    if hashlib.sha256(mission).hexdigest() != DESCRIPTOR_SHA256:
        raise ValueError("Descriptor differs from the verified Picasso release")
    hog = descriptor.with_name("picasso.hog").read_bytes()
    if hashlib.sha256(hog).hexdigest() != HOG_SHA256:
        raise ValueError("Sibling HOG differs from the verified Picasso release")

    corrected = mission.replace(b"\r\npicasso.rl2\r\n", b"\r\npicaso.rl2\r\n")
    write_mission_zip(output, (("picasso.mn2", corrected), ("picasso.hog", hog)))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("descriptor", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    try:
        recover(args.descriptor, args.output)
    except (OSError, ValueError) as exc:
        parser.exit(1, f"{exc}\n")
    print(f"Created {args.output} with the original Picasso HOG")
