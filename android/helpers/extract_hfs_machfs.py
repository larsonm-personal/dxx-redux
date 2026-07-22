#!/usr/bin/env python3
"""Extract a classic HFS image through machfs into a confined directory."""

import ntpath
import os
import shutil
import sys


class ExtractionBudget:
    def __init__(self, root_dir, max_entries, max_file_bytes, max_total_bytes, free_headroom):
        self.root_dir = root_dir
        self.max_entries = max_entries
        self.max_file_bytes = max_file_bytes
        self.max_total_bytes = max_total_bytes
        self.free_headroom = free_headroom
        self.entries = 0
        self.total_bytes = 0

    def reserve(self, size):
        self.entries += 1
        if self.entries > self.max_entries:
            raise ValueError(f"HFS catalog exceeds {self.max_entries} entries")
        if size > self.max_file_bytes:
            raise ValueError(f"HFS file exceeds {self.max_file_bytes} bytes")
        if self.total_bytes > self.max_total_bytes - size:
            raise ValueError(f"HFS output exceeds {self.max_total_bytes} bytes")
        if size and shutil.disk_usage(self.root_dir).free < size + self.free_headroom:
            raise ValueError("insufficient free space for HFS output")
        self.total_bytes += size


def checked_child_path(root_dir, parent_dir, name):
    if not isinstance(name, str) or not name or name in (".", ".."):
        raise ValueError(f"unsafe HFS catalog name: {name!r}")
    if "\x00" in name or "/" in name or "\\" in name:
        raise ValueError(f"unsafe HFS catalog name: {name!r}")
    if os.path.isabs(name) or ntpath.isabs(name) or ntpath.splitdrive(name)[0]:
        raise ValueError(f"unsafe HFS catalog name: {name!r}")

    root = os.path.realpath(root_dir)
    candidate = os.path.realpath(os.path.join(parent_dir, name))
    if candidate == root or os.path.commonpath((root, candidate)) != root:
        raise ValueError(f"HFS output escapes extraction root: {name!r}")
    return candidate


def extract_folder(folder, root_dir, out_dir, folder_type, file_type, budget, display_path=""):
    os.makedirs(out_dir, exist_ok=True)
    for name, obj in sorted(folder.items()):
        child_path = checked_child_path(root_dir, out_dir, name)
        full = display_path + "/" + name if display_path else name
        if isinstance(obj, folder_type):
            budget.reserve(0)
            extract_folder(obj, root_dir, child_path, folder_type, file_type, budget, full)
        elif isinstance(obj, file_type):
            data = obj.data or b""
            budget.reserve(len(data))
            if not data:
                continue
            with open(child_path, "wb") as output:
                output.write(data)
            print(f"  {full} ({len(data)} bytes)")


def main(argv):
    if len(argv) != 8:
        raise SystemExit(
            "usage: extract_hfs_machfs.py IMAGE OUTPUT_DIR MAX_IMAGE_BYTES "
            "MAX_ENTRIES MAX_FILE_BYTES MAX_TOTAL_BYTES FREE_HEADROOM"
        )

    import machfs

    img_path, out_dir = argv[1:3]
    max_image_bytes, max_entries, max_file_bytes, max_total_bytes, free_headroom = (
        int(value) for value in argv[3:]
    )
    if min(max_image_bytes, max_entries, max_file_bytes, max_total_bytes) <= 0 or free_headroom < 0:
        raise ValueError("invalid HFS extraction budget")
    if os.path.getsize(img_path) > max_image_bytes:
        raise ValueError(f"HFS image exceeds {max_image_bytes} bytes")
    with open(img_path, "rb") as image:
        img_data = image.read()

    volume = machfs.Volume()
    volume.read(img_data)
    print(f"Volume: {volume.name}")
    os.makedirs(out_dir, exist_ok=True)
    budget = ExtractionBudget(out_dir, max_entries, max_file_bytes, max_total_bytes, free_headroom)
    extract_folder(volume, out_dir, out_dir, machfs.Folder, machfs.File, budget)
    print("HFS extraction complete")


if __name__ == "__main__":
    main(sys.argv)
