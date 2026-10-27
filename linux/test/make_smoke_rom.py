#!/usr/bin/env python3
"""Writes the smoke test's GBA ROM: a header and an ARM infinite loop.

mGBA identifies a GBA ROM by two bytes only (0xEA at offset 3, the top byte of
the entry branch, and 0x96 at 0xB2), so no game data, boot logo or BIOS is
needed here and none is in the repository. The host's --test-fill option paints
the screen after loading, which proves the emulator, its PPU, the texture
upload, the scaling and the screenshot path all work.

    python3 linux/test/make_smoke_rom.py /tmp/smoke.gba
"""
import struct
import sys

SIZE = 256 * 1024

rom = bytearray(SIZE)
rom[0x00:0x04] = struct.pack('<I', 0xEA00002E)  # b 0xC0, from 0x08000000
rom[0xA0:0xAC] = b'POKEDAISY\0\0\0'             # title, 12 bytes
rom[0xAC:0xB0] = b'BPRE'                        # game code (FireRed's)
rom[0xB0:0xB2] = b'01'                          # maker code
rom[0xB2] = 0x96                                # the fixed header byte mGBA checks
rom[0xBD] = -(0x19 + sum(rom[0xA0:0xBD])) & 0xFF  # header checksum
rom[0xC0:0xC4] = struct.pack('<I', 0xEAFFFFFE)  # b . (an ARM infinite loop)

with open(sys.argv[1], 'wb') as f:
    f.write(rom)
