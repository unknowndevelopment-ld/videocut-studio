#!/usr/bin/env python3
"""Build the single-file installer.

Usage: make_payload.py <stage-dir> <stub-exe> <output-exe> <version> [bootstrap-dir]

Layout of the output file:
    [stub exe bytes]
    [bootstrap zip]                  <- setup UI + its Qt runtime (deflate zip)
    [uint32 BE index length][index JSON]
    [per-file blobs: uint32 BE size + zlib stream]   (qUncompress format)
    [trailer: "VCSP1000" | u64 bootOffset | u64 bootSize |
              u64 payloadOffset | u64 payloadSize | sha256(payload)]   (72 bytes)
"""
import hashlib
import io
import json
import os
import struct
import sys
import zipfile
import zlib

MAGIC = b"VCSP1000"


def build_bootstrap(directory: str) -> bytes:
    buffer = io.BytesIO()
    with zipfile.ZipFile(buffer, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for root, dirs, names in os.walk(directory):
            dirs.sort()
            for name in sorted(names):
                full = os.path.join(root, name)
                rel = os.path.relpath(full, directory).replace("\\", "/")
                archive.write(full, rel)
    return buffer.getvalue()


def main() -> int:
    if len(sys.argv) < 5:
        print(__doc__)
        return 2
    stage, stub, output, version = sys.argv[1:5]
    bootstrap_dir = sys.argv[5] if len(sys.argv) > 5 else ""

    bootstrap = build_bootstrap(bootstrap_dir) if bootstrap_dir else b""

    entries = []
    blobs = []
    total = 0
    for root, dirs, names in os.walk(stage):
        dirs.sort()
        for name in sorted(names):
            full = os.path.join(root, name)
            rel = os.path.relpath(full, stage).replace("\\", "/")
            with open(full, "rb") as fh:
                data = fh.read()
            compressed = struct.pack(">I", len(data)) + zlib.compress(data, 6)
            entries.append({"p": rel, "s": len(data), "c": len(compressed)})
            blobs.append(compressed)
            total += len(data)

    offset = 0
    for entry in entries:
        entry["o"] = offset
        offset += entry["c"]

    index = json.dumps({"version": version, "files": entries}, separators=(",", ":")).encode("utf-8")
    payload = struct.pack(">I", len(index)) + index + b"".join(blobs)

    stub_size = os.path.getsize(stub)
    boot_offset = stub_size
    payload_offset = stub_size + len(bootstrap)

    with open(output, "wb") as out:
        with open(stub, "rb") as src:
            while True:
                chunk = src.read(1 << 20)
                if not chunk:
                    break
                out.write(chunk)
        out.write(bootstrap)
        out.write(payload)
        out.write(MAGIC)
        out.write(struct.pack(">QQ", boot_offset, len(bootstrap)))
        out.write(struct.pack(">QQ", payload_offset, len(payload)))
        out.write(hashlib.sha256(payload).digest())

    print(f"bootstrap: {len(bootstrap) / 1e6:.1f} MB  payload: {len(entries)} files, "
          f"{total / 1e6:.1f} MB  ->  installer {os.path.getsize(output) / 1e6:.1f} MB")
    return 0


if __name__ == "__main__":
    sys.exit(main())
