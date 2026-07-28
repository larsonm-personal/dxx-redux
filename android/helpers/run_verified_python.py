import runpy
import sys
from pathlib import Path


def main() -> None:
    if len(sys.argv) < 3:
        raise SystemExit("usage: run_verified_python.py <module-dir> <script> [args...]")
    sys.dont_write_bytecode = True
    module_dir = str(Path(sys.argv[1]).resolve())
    script = str(Path(sys.argv[2]).resolve())
    sys.path.insert(0, module_dir)
    sys.argv = [script, *sys.argv[3:]]
    runpy.run_path(script, run_name="__main__")


if __name__ == "__main__":
    main()
