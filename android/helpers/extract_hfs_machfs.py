#!/usr/bin/env python3
"""Extract a classic HFS image through machfs into a confined directory."""

import ntpath
import os
import sys


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


def extract_folder(folder, root_dir, out_dir, folder_type, file_type, display_path=""):
    os.makedirs(out_dir, exist_ok=True)
    for name, obj in sorted(folder.items()):
        child_path = checked_child_path(root_dir, out_dir, name)
        full = display_path + "/" + name if display_path else name
        if isinstance(obj, folder_type):
            extract_folder(obj, root_dir, child_path, folder_type, file_type, full)
        elif isinstance(obj, file_type) and obj.data:
            with open(child_path, "wb") as output:
                output.write(obj.data)
            print(f"  {full} ({len(obj.data)} bytes)")


def main(argv):
    if len(argv) != 3:
        raise SystemExit("usage: extract_hfs_machfs.py IMAGE OUTPUT_DIR")

    import machfs

    img_path, out_dir = argv[1:]
    with open(img_path, "rb") as image:
        img_data = image.read()

    volume = machfs.Volume()
    volume.read(img_data)
    print(f"Volume: {volume.name}")
    os.makedirs(out_dir, exist_ok=True)
    extract_folder(volume, out_dir, out_dir, machfs.Folder, machfs.File)
    print("HFS extraction complete")


if __name__ == "__main__":
    main(sys.argv)
