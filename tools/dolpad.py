"""Pad every DOL section to a multiple of 32 bytes, in place.

elf2dol keeps section sizes as-is; Dolphin refuses unaligned ones
("Failed to init core"). Usage: python3 tools/dolpad.py file.dol
"""
import struct
import sys

path = sys.argv[1]
dol = open(path, 'rb').read()
hdr = list(struct.unpack('>64I', dol[:256]))
out = bytearray(256)

for i in range(18):  # 7 text + 11 data sections: offset[i], size[36 + i]
    if not hdr[36 + i]:
        continue
    sec = dol[hdr[i]:hdr[i] + hdr[36 + i]]
    sec += b'\0' * (-len(sec) % 32)
    hdr[i], hdr[36 + i] = len(out), len(sec)
    out += sec

out[:256] = struct.pack('>64I', *hdr)
open(path, 'wb').write(out)
