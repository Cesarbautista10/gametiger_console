/*
 * GameTiger stores high scores at flash offset 0x180000 and large Game Boy
 * ROMs at 0x200000. Fail the link instead of allowing future firmware growth
 * to overwrite either persistent region.
 */
ASSERT(__flash_binary_end <= 0x10180000,
       "GameTiger firmware overlaps the reserved high-score/ROM-cache flash")

