from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")

assert '#include "secrets.h"' in SOURCE
assert 'wifi_stream_init("' not in SOURCE
assert (ROOT / "include" / "secrets.example.h").is_file()
