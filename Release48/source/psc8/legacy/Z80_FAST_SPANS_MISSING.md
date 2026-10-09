# Missing generated include

The accepted CP03 `z80_gba.c` includes `z80_fast_spans.inc`, but that generated file was not retained in the compact CP03 archive and was not found in the Library.

The exact accepted optimized module is retained at:

`../../build_artifacts/PSC8_CP03.bin`

Do not create a silent no-op stub and call it equivalent; doing so changes performance and module bytes. Recover/regenerate the include before claiming a full source rebuild of PSC8.
