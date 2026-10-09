#!/usr/bin/env python3
"""Readers for the Mega Drive Nemesis, Enigma and Kosinski formats used by PSIV."""

from __future__ import annotations


class _BitReader:
    def __init__(self, data: bytes, offset: int = 0) -> None:
        self._data = data
        self._bit_position = offset * 8

    def peek(self, count: int) -> int:
        position = self._bit_position
        value = self.read(count)
        self._bit_position = position
        return value

    def read(self, count: int) -> int:
        if count < 0:
            raise ValueError("bit count cannot be negative")

        value = 0
        for _ in range(count):
            byte_index = self._bit_position // 8
            if byte_index >= len(self._data):
                raise ValueError("compressed stream ended unexpectedly")
            bit_index = 7 - (self._bit_position % 8)
            value = (value << 1) | ((self._data[byte_index] >> bit_index) & 1)
            self._bit_position += 1
        return value


def kosinski_decompress(source: bytes) -> bytes:
    """Decompress a PSIV Kosinski stream.

    Kosinski descriptor words are little-endian and consumed least-significant
    bit first.  Back references use negative offsets from the current output
    position.  A full-width match with an extended count of zero terminates
    the stream.
    """
    source_offset = 0
    descriptor = 0
    descriptor_bits = 0
    output = bytearray()

    def next_byte() -> int:
        nonlocal source_offset
        if source_offset >= len(source):
            raise ValueError("Kosinski stream ended unexpectedly")
        value = source[source_offset]
        source_offset += 1
        return value

    def next_bit() -> int:
        nonlocal descriptor, descriptor_bits
        if descriptor_bits == 0:
            low = next_byte()
            high = next_byte()
            descriptor = low | (high << 8)
            descriptor_bits = 16
        value = descriptor & 1
        descriptor >>= 1
        descriptor_bits -= 1
        # The 68000 routine fetches the next descriptor immediately after
        # shifting the 16th bit, before it consumes bytes belonging to that
        # bit's literal or match.  Kosinski streams interleave data around
        # this eager reload, so deferring it changes the stream alignment.
        if descriptor_bits == 0:
            low = next_byte()
            high = next_byte()
            descriptor = low | (high << 8)
            descriptor_bits = 16
        return value

    def copy_match(offset: int, count: int) -> None:
        if offset >= 0 or -offset > len(output):
            raise ValueError(f"invalid Kosinski back reference: {offset}")
        for _ in range(count):
            output.append(output[len(output) + offset])

    while True:
        if next_bit():
            output.append(next_byte())
            continue

        if not next_bit():
            count = (next_bit() << 1) | next_bit()
            offset = next_byte() - 0x100
            copy_match(offset, count + 2)
            continue

        low = next_byte()
        high = next_byte()
        offset_word = 0xE000 | ((high & 0xF8) << 5) | low
        offset = offset_word - 0x10000
        count = high & 7

        if count:
            copy_match(offset, count + 2)
            continue

        count = next_byte()
        if count == 0:
            return bytes(output)
        if count == 1:
            continue
        copy_match(offset, count + 1)

def nemesis_decompress(source: bytes) -> bytes:
    """Decompress a Sega Nemesis stream to Mega Drive 4bpp tiles.

    This follows the format's 256-entry prefix lookup literally: code bytes in
    the table store the numeric prefix in their *low* bits, and that value is
    shifted into the high side of an 8-bit lookup index.  Keeping the parser
    shaped like the original decoder avoids the ambiguity that caused earlier
    experiments with code alignment.
    """
    if len(source) < 4:
        raise ValueError("Nemesis stream is too short")

    header = int.from_bytes(source[0:2], "big")
    xor_mode = bool(header & 0x8000)
    tile_count = header & 0x7FFF
    if not tile_count:
        return b""

    offset = 2
    nibble_value = 0
    # index -> (total code bits, nibble value, run length)
    table: dict[int, tuple[int, int, int]] = {}

    def next_byte() -> int:
        nonlocal offset
        if offset >= len(source):
            raise ValueError("Nemesis code table ended unexpectedly")
        value = source[offset]
        offset += 1
        return value

    byte = next_byte()
    while byte != 0xFF:
        if byte & 0x80:
            nibble_value = byte & 0x0F
            byte = next_byte()
            continue

        run_length = ((byte >> 4) & 0x07) + 1
        total_code_bits = byte & 0x0F
        if not 1 <= total_code_bits <= 8:
            raise ValueError(f"invalid Nemesis code length: {total_code_bits}")
        code = next_byte()
        table_index = code << (8 - total_code_bits)
        if table_index >= 256:
            raise ValueError(
                f"invalid Nemesis code byte 0x{code:02X} for {total_code_bits}-bit code")
        table[table_index] = (total_code_bits, nibble_value, run_length)
        byte = next_byte()

    reader = _BitReader(source, offset)
    output = bytearray()
    output_row = 0
    output_row_nibbles = 0
    previous_row = 0
    nibbles_remaining = tile_count * 8 * 8

    def output_nibble(nibble: int) -> None:
        nonlocal output_row, output_row_nibbles, previous_row
        output_row = ((output_row << 4) | (nibble & 0x0F)) & 0xFFFFFFFF
        output_row_nibbles += 1
        if output_row_nibbles == 8:
            final_row = output_row ^ previous_row if xor_mode else output_row
            output.extend(final_row.to_bytes(4, "big"))
            previous_row = final_row
            output_row = 0
            output_row_nibbles = 0

    while nibbles_remaining:
        code = 0
        matched: tuple[int, int] | None = None
        for total_code_bits in range(1, 9):
            code = (code << 1) | reader.read(1)

            # 111111 is the reserved inline run marker.
            if total_code_bits == 6 and code == 0x3F:
                run_length = reader.read(3) + 1
                nibble = reader.read(4)
                matched = (run_length, nibble)
                break

            entry = table.get(code << (8 - total_code_bits))
            if entry is not None and entry[0] == total_code_bits:
                matched = (entry[2], entry[1])
                break

        if matched is None:
            raise ValueError("Nemesis stream contains an unknown prefix code")

        run_length, nibble = matched
        if run_length > nibbles_remaining:
            raise ValueError("Nemesis run exceeds the size declared by the header")
        for _ in range(run_length):
            output_nibble(nibble)
        nibbles_remaining -= run_length

    if output_row_nibbles:
        raise ValueError("Nemesis stream ended on a partial 32-bit output row")
    return bytes(output)


def enigma_decompress(source: bytes, starting_tile: int = 0) -> list[int]:
    """Decompress a PSIV Enigma plane map to 16-bit Mega Drive cells."""
    if len(source) < 8:
        raise ValueError("Enigma stream is too short")

    inline_bits = source[0]
    flags = source[1]
    incremental = (int.from_bytes(source[2:4], "big") + starting_tile) & 0xFFFF
    literal = (int.from_bytes(source[4:6], "big") + starting_tile) & 0xFFFF
    reader = _BitReader(source, 6)
    output: list[int] = []

    def read_inline() -> int:
        # Enigma stores the inline-attribute *presence mask* in the low five
        # bits of header byte 1.  Mask bits 4..0 correspond to Mega Drive
        # pattern-name bits 15..11 (priority, palette 1, palette 0, V-flip,
        # H-flip).  Only attributes enabled by that mask consume payload bits.
        #
        # This is easy to get backwards because the resulting attributes live
        # in the high five bits of the 16-bit tile word.  Treating header bits
        # 7..3 as the mask desynchronizes real PSIV streams such as the title
        # screen mappings and eventually runs the bit reader past EOF.
        value = starting_tile
        for mask, attribute in (
                (0x10, 0x8000),
                (0x08, 0x4000),
                (0x04, 0x2000),
                (0x02, 0x1000),
                (0x01, 0x0800)):
            if flags & mask and reader.read(1):
                value |= attribute
        return (value + reader.read(inline_bits)) & 0xFFFF

    while True:
        leading = reader.peek(2)
        if leading < 2:
            kind = reader.read(2)
        else:
            kind = reader.read(3)
        repeat = reader.read(4)
        count = repeat + 1

        if kind == 0:
            for _ in range(count):
                output.append(incremental)
                incremental = (incremental + 1) & 0xFFFF
        elif kind == 1:
            output.extend([literal] * count)
        elif kind == 4:
            output.extend([read_inline()] * count)
        elif kind == 5:
            value = read_inline()
            for _ in range(count):
                output.append(value)
                value = (value + 1) & 0xFFFF
        elif kind == 6:
            value = read_inline()
            for _ in range(count):
                output.append(value)
                value = (value - 1) & 0xFFFF
        elif kind == 7:
            if repeat == 0x0F:
                return output
            for _ in range(count):
                output.append(read_inline())
        else:
            raise ValueError(f"invalid Enigma entry type: {kind}")
