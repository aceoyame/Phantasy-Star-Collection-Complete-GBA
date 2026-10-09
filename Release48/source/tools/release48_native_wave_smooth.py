#!/usr/bin/env python3
"""Zero-runtime-cost PSII/PSIII native-wave smoothing for Release 48.

PSC stores the native PSII/PSIII source waves as signed logarithmic bytes and
samples them with a Q14 zero-order hold.  This patch preconditions each normal
positive-count source wave by replacing sample i with a linear-amplitude blend
between sample i and i+1, then re-encodes the result to the nearest legal
signed-log byte.  Wave pointers, lengths, tables, pitch, envelopes, mixer,
DMA timing and ROM size do not change.

The final byte of each declared wave is retained verbatim so we never guess
past a table-defined waveform boundary.  PSIII count=-8 special rows are included using their native one-cycle length;
unused/sentinel rows remain excluded. Exact duplicate physical PSIII wave intervals
are transformed only once.
"""
from __future__ import annotations
from bisect import bisect_left
from dataclasses import dataclass
from pathlib import Path
import math
import struct

ROM_BASE = 0x08000000
PSII_TABLE_OFF = 0x481510
PSIII_TABLE_OFF = 0x152080
PSIII_LOOKUP_OFF = 0x269AF0
PSII_LOOKUP_PTR_OFF = 0x3953EC

@dataclass(frozen=True)
class WaveInterval:
    game: str
    index: int
    off: int
    length: int


def _s8(v: int) -> int:
    return v if v < 128 else v - 256


def _lookup_offset(shell: bytes, game: str) -> int:
    if game == "III":
        return PSIII_LOOKUP_OFF
    if game == "II":
        return struct.unpack_from('<I', shell, PSII_LOOKUP_PTR_OFF)[0] - ROM_BASE
    raise ValueError(game)


def _decode_table(shell: bytes, game: str) -> list[int]:
    """Return amplitude for each encoded raw byte 0..255.

    The ROM lookup is biased by 0x80 relative to the signed-log byte stored in
    waves/scratch.  Normal source waves use only signed codes -119..119.
    """
    off = _lookup_offset(shell, game)
    raw = [struct.unpack_from('<i', shell, off + i * 4)[0] for i in range(256)]
    return [raw[(q + 128) & 0xFF] for q in range(256)]


def _encoder(shell: bytes, game: str):
    dec = _decode_table(shell, game)
    # Restrict output to the normal signed-log domain used by audited waves.
    pairs = sorted((dec[q & 0xFF], q & 0xFF) for q in range(-120, 121))
    amps = [a for a, _ in pairs]
    codes = [q for _, q in pairs]

    def encode(amp: int) -> int:
        amp = max(-32768, min(32767, int(amp)))
        j = bisect_left(amps, amp)
        if j <= 0:
            return codes[0]
        if j >= len(amps):
            return codes[-1]
        lo, hi = j - 1, j
        # Stable tie break toward the quieter/smaller-magnitude code.
        dlo, dhi = abs(amps[lo] - amp), abs(amps[hi] - amp)
        if dlo < dhi:
            return codes[lo]
        if dhi < dlo:
            return codes[hi]
        return min((codes[lo], codes[hi]), key=lambda q: abs(_s8(q)))

    return dec, encode


def _wave_intervals(shell: bytes, game: str) -> list[WaveInterval]:
    rows: list[WaveInterval] = []
    if game == "II":
        off, stride = PSII_TABLE_OFF, 8
        for i in range(256):
            count, ptr = struct.unpack_from('<II', shell, off + i * stride)
            if count >= 0x80000000 or count <= 0:
                continue
            p = ptr - ROM_BASE
            if p < 0 or p + count > len(shell):
                raise ValueError(f'PSII row {i}: wave outside shell: {p:#x}+{count:#x}')
            rows.append(WaveInterval(game, i, p, count))
    elif game == "III":
        off, stride = PSIII_TABLE_OFF, 60
        for i in range(256):
            count, ptr, phase = struct.unpack_from('<III', shell, off + i * stride)
            if count == 0xFFFFFFF8:
                # PSC's PSIII note initializer treats -8 as a valid special
                # wave: its table phase-cycle encodes the one-cycle byte length.
                length = phase >> 14
            elif count >= 0x80000000 or count <= 0:
                continue
            else:
                length = (phase >> 14) * count
            if length <= 0:
                continue
            p = ptr - ROM_BASE
            if p < 0 or p + length > len(shell):
                raise ValueError(f'PSIII row {i}: wave outside shell: {p:#x}+{length:#x}')
            rows.append(WaveInterval(game, i, p, length))
    else:
        raise ValueError(game)
    return rows


def apply(shell: bytes, blend_num: int = 1, blend_den: int = 4, *, preserve_rms: bool = False, gain_cap: float = 1.08) -> tuple[bytes, dict]:
    """Apply an offline linear-amplitude forward blend.

    blend_num/blend_den is the contribution of sample i+1.  1/4 is the
    conservative default. Arithmetic is integer round-to-nearest. When
    preserve_rms is enabled, each unique wave receives a bounded post-blend
    gain so its transformed N-1 samples retain the authored preblend RMS.
    """
    if len(shell) != 0x800000:
        raise ValueError(f'expected 8 MiB PSC shell, got {len(shell):#x}')
    if not (0 < blend_num < blend_den):
        raise ValueError('blend must be strictly between 0 and 1')

    out = bytearray(shell)
    report = {
        'method': 'offline linear-amplitude forward blend, nearest signed-log re-encode',
        'blend': f'{blend_num}/{blend_den}',
        'runtime_cost': 'none; tables/pointers/lengths/phase kernels unchanged',
        'preserve_rms': preserve_rms,
        'gain_cap': gain_cap if preserve_rms else None,
        'games': {},
    }

    for game in ('II', 'III'):
        dec, encode = _encoder(shell, game)
        rows = _wave_intervals(shell, game)
        # Exact physical duplicates exist in PSIII.  Do not transform twice.
        unique: dict[tuple[int, int], WaveInterval] = {}
        for r in rows:
            unique.setdefault((r.off, r.length), r)
        changed = 0
        considered = 0
        max_amp_delta = 0
        encoded_abs_delta_sum = 0
        gains: list[float] = []
        gain_capped = 0
        for (off, length), row in sorted(unique.items()):
            if length < 2:
                continue
            src = shell[off:off + length]
            dst = bytearray(src)
            authored = [dec[q] for q in src]
            blended: list[int] = []
            for i in range(length - 1):
                a = authored[i]
                b = authored[i + 1]
                numer = a * (blend_den - blend_num) + b * blend_num
                if numer >= 0:
                    mixed = (numer + blend_den // 2) // blend_den
                else:
                    mixed = -((-numer + blend_den // 2) // blend_den)
                blended.append(mixed)
            gain = 1.0
            if preserve_rms:
                src_rms = math.sqrt(sum(x*x for x in authored[:-1]) / (length - 1))
                dst_rms = math.sqrt(sum(x*x for x in blended) / (length - 1))
                if dst_rms > 0:
                    gain = min(gain_cap, src_rms / dst_rms)
                if gain >= gain_cap - 1e-12:
                    gain_capped += 1
            gains.append(gain)
            for i, mixed in enumerate(blended):
                if gain != 1.0:
                    scaled = mixed * gain
                    mixed = int(math.floor(scaled + 0.5)) if scaled >= 0 else -int(math.floor(-scaled + 0.5))
                q = encode(mixed)
                considered += 1
                if q != src[i]:
                    changed += 1
                    encoded_abs_delta_sum += abs(_s8(q) - _s8(src[i]))
                max_amp_delta = max(max_amp_delta, abs(dec[q] - authored[i]))
                dst[i] = q
            # Keep final byte exactly as authored.
            out[off:off + length] = dst
        report['games'][game] = {
            'table_rows': len(rows),
            'unique_physical_intervals': len(unique),
            'logical_wave_bytes': sum(r.length for r in rows),
            'unique_physical_wave_bytes': sum(length for _, length in unique),
            'samples_considered': considered,
            'bytes_changed': changed,
            'mean_abs_log_code_delta_changed_only': (encoded_abs_delta_sum / changed if changed else 0.0),
            'max_decoded_amplitude_delta': max_amp_delta,
            'mean_postblend_gain': (sum(gains) / len(gains) if gains else 1.0),
            'max_postblend_gain': (max(gains) if gains else 1.0),
            'gain_capped_intervals': gain_capped,
        }

    return bytes(out), report


if __name__ == '__main__':
    import argparse, json, zlib
    ap = argparse.ArgumentParser()
    ap.add_argument('input', type=Path)
    ap.add_argument('output', type=Path)
    ap.add_argument('--blend-num', type=int, default=1)
    ap.add_argument('--blend-den', type=int, default=4)
    ap.add_argument('--preserve-rms', action='store_true')
    ap.add_argument('--gain-cap', type=float, default=1.08)
    a = ap.parse_args()
    raw = a.input.read_bytes()
    patched, report = apply(raw[:0x800000], a.blend_num, a.blend_den, preserve_rms=a.preserve_rms, gain_cap=a.gain_cap)
    result = patched + raw[0x800000:]
    a.output.write_bytes(result)
    report['base_crc32'] = f'{__import__("zlib").crc32(raw) & 0xffffffff:08x}'
    report['candidate_crc32'] = f'{__import__("zlib").crc32(result) & 0xffffffff:08x}'
    print(json.dumps(report, indent=2))
