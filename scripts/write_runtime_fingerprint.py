#!/usr/bin/env python3
"""Embed a reproducible identity for the exact patched runtime in nx.log."""
from hashlib import sha256
from pathlib import Path
import sys

root = Path(sys.argv[1]).resolve()
upstream = sys.argv[2]
adapter = sys.argv[3]

runtime = root / "runtime"
files = sorted(
    p for base in (runtime / "src", runtime / "include", root / "games" / "p5s")
    if base.exists()
    for p in base.rglob("*")
    if p.is_file() and p.name != "p5s_build_id.h"
)
digest = sha256()
for path in files:
    rel = path.relative_to(root).as_posix().encode()
    data = path.read_bytes()
    digest.update(len(rel).to_bytes(4, "little"))
    digest.update(rel)
    digest.update(len(data).to_bytes(8, "little"))
    digest.update(data)
source_hash = digest.hexdigest()

header = runtime / "src" / "p5s_build_id.h"
header.write_text(
    "#pragma once\n"
    f'#define P5S_UPSTREAM_RUNTIME_COMMIT "{upstream}"\n'
    f'#define P5S_ADAPTER_COMMIT "{adapter}"\n'
    f'#define P5S_RUNTIME_SOURCE_SHA256 "{source_hash}"\n'
)

main = runtime / "src" / "main.cpp"
text = main.read_text()
include = '#include "p5s_build_id.h"'
if include not in text:
    anchor = '#include "platform.h"'
    if text.count(anchor) != 1:
        raise SystemExit("main.cpp: unique platform.h include not found")
    text = text.replace(anchor, anchor + "\n" + include, 1)
log_line = '    log("build: P5S upstream=%s adapter=%s runtime-src-sha256=%s", P5S_UPSTREAM_RUNTIME_COMMIT, P5S_ADAPTER_COMMIT, P5S_RUNTIME_SOURCE_SHA256);'
if log_line not in text:
    anchor = "void boot() {"
    if text.count(anchor) != 1:
        raise SystemExit("main.cpp: unique boot() entry not found")
    text = text.replace(anchor, anchor + "\n" + log_line, 1)
main.write_text(text)
print(f"P5S runtime identity embedded: upstream={upstream} adapter={adapter} sha256={source_hash}")
