"""Compatibility entry; Release 48 build is authoritative."""
import runpy
from pathlib import Path
runpy.run_path(str(Path(__file__).with_name("build_release48.py")),run_name="__main__")
