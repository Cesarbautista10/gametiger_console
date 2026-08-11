# Peanut-GB host smoke test

The smoke test loads ROM paths supplied on the command line, initialises the
same Peanut-GB header used by the firmware, renders frames without hardware,
and fails on core errors, out-of-bounds cartridge accesses, ASan findings, or
UBSan findings. MBC5 ROMs also get a direct multi-bank cartridge-RAM
read/write check to protect battery saves. ROM files are test inputs and are
not stored in this repository.

Run it from the repository root:

```sh
tests/run_peanut_gb_smoke.sh --frames 18000 \
  /path/to/Pokemon.gb /path/to/SpaceInvaders.gb /path/to/StreetFighterII.gb
```

`--frames` is optional and defaults to 3000 frames per ROM. Set `CC=clang` (or
another sanitizer-capable C compiler) to override the default host compiler.
