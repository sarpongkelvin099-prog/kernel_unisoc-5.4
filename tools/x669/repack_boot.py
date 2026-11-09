#!/usr/bin/env python3
"""Replace the kernel inside a stock Android boot.img (header v4) and keep the ramdisk.

usage: repack_boot.py stock_boot.img new_Image out_boot.img
The GKI boot signature and AVB footer are dropped; flash vbmeta with verification disabled (see device tree docs/flashing.md).
"""
import struct, sys

PAGE = 4096
def align(x): return (x + PAGE - 1) // PAGE * PAGE

stock, image, out = sys.argv[1:4]
d = open(stock, "rb").read()
assert d[:8] == b"ANDROID!", "not a boot image"
ks, rs, osv, hs = struct.unpack_from("<4I", d, 8)
ver = struct.unpack_from("<I", d, 40)[0]
assert ver == 4, f"header v{ver}, expected 4"
sig = struct.unpack_from("<I", d, 1580)[0] if len(d) > 1584 else 0  # boot_signature_size (v4)
ramdisk = d[PAGE + align(ks):PAGE + align(ks) + rs]
new_k = open(image, "rb").read()
hdr = bytearray(d[:PAGE])
struct.pack_into("<I", hdr, 8, len(new_k))
struct.pack_into("<I", hdr, 1580, 0)  # no GKI boot signature after the ramdisk
body = bytes(hdr) + new_k + b"\0" * (align(len(new_k)) - len(new_k)) + ramdisk
body += b"\0" * (align(len(body)) - len(body))
open(out, "wb").write(body)
print(f"kernel {ks} -> {len(new_k)} bytes, ramdisk {rs}, out {len(body)} bytes (boot_signature_size field={sig})")
