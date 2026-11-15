#!/usr/bin/env python3
"""Turn doom1.wad into the MRAM image the firmware maps: sound and music data dropped, lumps 4-byte aligned, CRC header."""

import sys

sys.dont_write_bytecode = True

import argparse
import struct
import zlib

HEADER_MAGIC = b"DWAD"
HEADER_SIZE = 16
# Directory entries stay so lookups such as S_ChangeMusic's W_GetNumForName("D_E1M1") still resolve; only the data goes.
DROP_PREFIXES = (b"DS", b"DP", b"D_")
ALIGN = 4


def read_wad(data):
    ident, count, dir_ofs = struct.unpack_from("<4sii", data, 0)
    if ident not in (b"IWAD", b"PWAD"):
        sys.exit(f"wadprep: not a WAD (ident {ident!r})")
    lumps = []
    for i in range(count):
        pos, size, raw_name = struct.unpack_from("<ii8s", data, dir_ofs + 16 * i)
        lumps.append((raw_name, data[pos:pos + size]))
    return ident, lumps


def build_wad(ident, lumps):
    out = bytearray(12)
    directory = []
    dropped = 0
    for raw_name, payload in lumps:
        name = raw_name.rstrip(b"\0")
        if name.startswith(DROP_PREFIXES):
            dropped += len(payload)
            payload = b""
        out += b"\0" * (-len(out) % ALIGN)
        directory.append(struct.pack("<ii8s", len(out), len(payload), raw_name))
        out += payload
    out += b"\0" * (-len(out) % ALIGN)
    dir_ofs = len(out)
    out += b"".join(directory)
    struct.pack_into("<4sii", out, 0, ident, len(lumps), dir_ofs)
    return bytes(out), dropped


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--in", dest="src", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--max-size", type=int, required=True, help="bytes available in the MRAM WAD region")
    args = parser.parse_args()

    with open(args.src, "rb") as f:
        original = f.read()
    ident, lumps = read_wad(original)
    wad, dropped = build_wad(ident, lumps)
    image = struct.pack("<4sIII", HEADER_MAGIC, len(wad), zlib.crc32(wad), 0) + wad
    if len(image) > args.max_size:
        sys.exit(f"wadprep: image {len(image)} bytes > WAD region {args.max_size} bytes")
    with open(args.out, "wb") as f:
        f.write(image)
    print(f"wadprep: {len(original)} -> {len(image)} bytes ({len(lumps)} lumps, {dropped} bytes of sound/music dropped, "
          f"{args.max_size - len(image)} bytes left in region)")


if __name__ == "__main__":
    main()
