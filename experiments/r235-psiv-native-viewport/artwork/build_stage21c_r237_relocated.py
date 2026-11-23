#!/usr/bin/env python3
"""Rebase Stage21C PSIV native viewport onto the accepted R237 gameplay fixes.

Uses a privately held, SHA-pinned R235, Stage14, Stage21C, and the user's R237
ZIP. Never commits ROM or assets. Resolves the 0x0922B000 graphics/code conflict
by recompiling R237 palette/Fin/title ARM helpers at a verified blank location.
This is a BINARY INTEGRATION ONLY; both emulator cores must still replay it.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import zipfile
import zlib

BASE = 0x08000000
N = 33554432
R235 = "4caf23ae8b030c16a32658fdc95085330de6c6dd4a51498f0e2b9c17c7d89308"
R237 = "a84e9c0501c16a2b750a85037cfa16bcd859101fb4b201ff4edaa1da6b2742de"
STAGE14 = "c1ed9db09505fe2d17322efa9f7c4915e7a4cd57bdf2a3dc6f24d0f8f1d15609"
STAGE21C = "ff5f96aae1a536ee13994040ff98bb74a2afbfe1f1a3065df6e996aa1cab5139"
OLD_BEGIN, OLD_END = 0x0922AF00, 0x0922BA00
NEW_BEGIN = 0x0924CE40
SOURCES = (("palfix.c", "r237_palfix", "r237_reload_palette_with_managers"),
           ("finfix.c", "r237_finfix", "r237_fin_entry"),
           ("titlefix.c", "r237_titlefix", "r237_title_startonly_hook"))
CALLS = {"r237_reload_palette_with_managers": (0x0882F234, 0x08822CBC),
         "r237_fin_entry": (0x08808CC8, 0x08808CE4),
         "r237_title_startonly_hook": (0x08801214,)}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def load(path, expected):
    data = Path(path).read_bytes()
    if len(data) != N or sha(data) != expected:
        raise ValueError("ROM size/hash mismatch: " + str(path))
    return data


def patch_ips32(base, patch):
    if patch[:5] != b"IPS32":
        raise ValueError("IPS32 magic missing")
    out = bytearray(base)
    at = 5
    while True:
        if at + 4 > len(patch):
            raise ValueError("truncated IPS32")
        if patch[at:at + 4] == b"EEOF":
            if at + 4 != len(patch):
                raise ValueError("unexpected trailing patch bytes")
            return bytes(out)
        pos = int.from_bytes(patch[at:at + 4], "big")
        size = int.from_bytes(patch[at + 4:at + 6], "big")
        at += 6
        if size:
            replacement = patch[at:at + size]
            at += size
        else:
            if at + 3 > len(patch):
                raise ValueError("truncated RLE")
            size = int.from_bytes(patch[at:at + 2], "big")
            replacement = bytes([patch[at + 2]]) * size
            at += 3
        if size == 0 or pos + size > N or len(replacement) != size:
            raise ValueError("IPS32 record outside 32 MiB")
        out[pos:pos + size] = replacement


def bl(pc, dst):
    delta = dst - (pc + 8)
    if delta & 3 or not -(1 << 25) <= delta < (1 << 25):
        raise ValueError("ARM BL out of range")
    return 0xEB000000 | ((delta >> 2) & 0xFFFFFF)


def target(pc, word):
    if word >> 24 != 0xEB:
        raise ValueError("not ARM BL at " + hex(pc))
    imm = word & 0xFFFFFF
    if imm & 0x800000:
        imm -= 0x1000000
    return pc + 8 + imm * 4


def word(rom, addr):
    return struct.unpack_from("<I", rom, addr - BASE)[0]


def compile_at(work, src, section, symbol, addr, tools):
    c, ld, objcopy, nm = tools
    obj, elf, binary, script = (work / ("helper" + ext)
                                for ext in (".o", ".elf", ".bin", ".ld"))
    script.write_text(
        f"SECTIONS {{ .{section} 0x{addr:08X} : "
        f"{{ *(.{section}) *(.text*) *(.rodata*) *(.data*) *(.bss*) }} "
        "/DISCARD/ : { *(.ARM.attributes) *(.comment) } }\n")
    subprocess.run([c, "--target=arm-none-eabi", "-mcpu=arm7tdmi", "-marm",
                    "-O2", "-ffreestanding", "-fno-builtin", "-c",
                    str(work / src), "-o", str(obj)], check=True)
    subprocess.run([ld, "-T", str(script), "-e", symbol,
                    str(obj), "-o", str(elf)], check=True)
    subprocess.run([objcopy, "-O", "binary", "--only-section=." + section,
                    str(elf), str(binary)], check=True)
    output = subprocess.check_output([nm, str(elf)], text=True)
    symbols = {x[2]: int(x[0], 16) for line in output.splitlines()
               if len(x := line.split()) == 3 and
               all(ch in "0123456789abcdefABCDEF" for ch in x[0])}
    blob = binary.read_bytes()
    if symbol not in symbols or not addr <= symbols[symbol] < addr + len(blob):
        raise ValueError("bad ARM symbol " + symbol)
    return blob, symbols[symbol]


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    for flag in ("r235", "stage14", "stage21c", "r237_zip", "out", "report"):
        ap.add_argument("--" + flag.replace("_", "-"), type=Path, required=True)
    a = ap.parse_args()
    if a.out.exists() or a.out.resolve() in [p.resolve() for p in
                                               (a.r235, a.stage14, a.stage21c)]:
        raise ValueError("refusing to overwrite an input or existing ROM")
    base = load(a.r235, R235)
    prior = load(a.stage14, STAGE14)
    experiment = load(a.stage21c, STAGE21C)
    # The OAM experiment must be the exact Stage14 parent + 22 changed bytes.
    allowed = {i for addr, count in ((0x08C7D4F4, 4), (0x08CD3854, 4),
                                      (0x08F97F90, 16))
               for i in range(addr - BASE, addr - BASE + count)}
    changed = {i for i, (before, after) in enumerate(zip(prior, experiment))
               if before != after}
    if len(changed) != 22 or not changed <= allowed:
        raise ValueError("Stage21C differs unexpectedly from Stage14")
    with zipfile.ZipFile(a.r237_zip) as z:
        candidate = patch_ips32(base, z.read("r237/R235_to_R237.ips32"))
        if sha(candidate) != R237 or candidate != z.read("r237/PSC_R237_CANDIDATE.gba"):
            raise ValueError("R237 patch does not rebuild the user candidate")
        with tempfile.TemporaryDirectory(prefix="psc_r237_reloc_") as tmp:
            work = Path(tmp)
            for source, _, _ in SOURCES:
                (work / source).write_bytes(z.read("r237/build/" + source))
            first, last = OLD_BEGIN - BASE, OLD_END - BASE
            collisions = [i for i in range(N) if base[i] != experiment[i] and
                          base[i] != candidate[i] and experiment[i] != candidate[i]]
            if not collisions or any(not first <= i < last for i in collisions):
                raise ValueError("unexpected rebase collision outside R237 payload")
            out = bytearray(candidate)
            # The original R237 routines occupy Stage14 artwork space. Restore
            # the full experiment slice, including original zeros, before
            # installing equivalent rebuilt R237 code elsewhere.
            out[first:last] = experiment[first:last]
            for i in range(N):
                if first <= i < last or base[i] == experiment[i]:
                    continue
                if base[i] != candidate[i] and candidate[i] != experiment[i]:
                    raise ValueError("unhandled shared edits at " + hex(i))
                out[i] = experiment[i]
            tools = [shutil.which(name) for name in
                     ("clang", "ld.lld", "llvm-objcopy")]
            tools.append(shutil.which("llvm-nm") or shutil.which("nm"))
            if not all(tools):
                raise ValueError("LLVM ARM toolchain and nm required")
            if any(base[i] or experiment[i] or candidate[i] for i in
                   range(NEW_BEGIN - BASE, NEW_BEGIN - BASE + 8192)):
                raise ValueError("relocation destination not uniformly blank")
            symbols, report_symbols = {}, {}
            address = NEW_BEGIN
            for source, section, symbol in SOURCES:
                address = (address + 63) & ~63
                blob, entry = compile_at(work, source, section, symbol, address, tools)
                pos = address - BASE
                if pos + len(blob) > N or any(out[pos:pos + len(blob)]):
                    raise ValueError("helper output overlaps nonblank ROM data")
                out[pos:pos + len(blob)] = blob
                symbols[symbol] = entry
                report_symbols[symbol] = {"base": hex(address), "entry": hex(entry),
                                          "size": len(blob), "sha256": sha(blob)}
                address += len(blob)
            retargeted = {}
            for symbol, calls in CALLS.items():
                for pc in calls:
                    old = target(pc, word(candidate, pc))
                    if not OLD_BEGIN <= old < OLD_END:
                        raise ValueError("original BL did not target old stub")
                    struct.pack_into("<I", out, pc - BASE, bl(pc, symbols[symbol]))
                    if target(pc, word(out, pc)) != symbols[symbol]:
                        raise ValueError("relocated ARM BL round trip failed")
                    retargeted[hex(pc)] = {"old": hex(old), "new": hex(symbols[symbol])}
            # Verify existing R237 stack, audio, and Saya calls were preserved.
            for addr in (0x088001B0, 0x088001B4, 0x0881B6EC,
                         0x0881BA1C, 0x08866F70, 0x08866FF4):
                if word(out, addr) != word(candidate, addr):
                    raise ValueError("R237 gameplay code changed " + hex(addr))
            for addr, n in ((0x08C7D4F4, 4), (0x08CD3854, 4), (0x08F97F90, 16)):
                pos = addr - BASE
                if out[pos:pos + n] != experiment[pos:pos + n]:
                    raise ValueError("Stage21C camera code changed")
            if out[first:last] != experiment[first:last] or out[:0xC0] != base[:0xC0]:
                raise ValueError("Stage21C art or GBA header changed")
            report = {
                "classification": "EXPERIMENTAL_UNTESTED_NOT_PROMOTED",
                "accepted_r235_sha256": R235, "accepted_r237_sha256": R237,
                "stage14_sha256": STAGE14, "stage21c_sha256": STAGE21C,
                "size_bytes": N, "candidate_sha256": sha(out),
                "candidate_crc32": f"{zlib.crc32(out):08x}",
                "overlap_conflict_bytes_relocated": len(collisions),
                "relocated_functions": report_symbols, "retargeted_arm_bl": retargeted,
                "qa": "STATIC_ONLY: live mGBA, VBA-M/Delta and Academy/Fin not yet replayed",
            }
            a.out.parent.mkdir(parents=True, exist_ok=True)
            a.report.parent.mkdir(parents=True, exist_ok=True)
            a.out.write_bytes(out)
            a.report.write_text(json.dumps(report, indent=2) + "\n")
            print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
