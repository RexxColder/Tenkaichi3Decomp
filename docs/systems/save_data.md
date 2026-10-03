# Save data

Source: `src/sys/save.c`. Layout: `include/sys/save.h` (`SaveData`, exactly 0x4000 bytes).

`gSaveData` points at a 0x4000-byte heap block that is the whole persistent save, not just the
options: the memory card code copies 0x4000 bytes into it after a load and passes it whole when
saving.

## Layout (verified by matching code unless marked)

| Offset | Field | Notes |
|---|---|---|
| 0x0008 | `unlockFlags` | 7 bits (plus bit 9 used by the menu); meaning unknown |
| 0x0010 | `slot[9]`, 0x10 each | flags + three values; probably per-mode progress (inferred) |
| 0x0C10 | `charaBits[3]` (u64) | 161 character unlock bits |
| 0x0C28 | `stageBits` (u64) | 35 stage unlock bits |
| 0x0C30 | `bgmBits` | 20 bits of a 25-entry list; "BGM" is a guess |
| 0x0C34 | `rule[6]` | "battle rules" is a guess; defaults 3, 2, 2, 0, 0, 0 |
| 0x1608 | `flags` | see below |
| 0x160C | `key[2][8]` | battle button assignment per player |
| 0x164C | `keyEdit[2][8]` | the copy the controller menu edits |
| 0x1694, 0x1698 | unknown | copied per side into the battle option block |
| 0x169C, 0x16A0 | `screenX`, `screenY` | display position |
| 0x16A4 | `soundMode` | 0 stereo, 1 mono |
| 0x16A8 | `bgmVolume` | 0..9 |
| 0x16AC | `seVolume` | 0..9 |
| 0x1808 | `custom[97]`, 0x38 each | three sets of seven item slots, plus a level |
| 0x2D40 | `rec[14]`, 0x1C each | level and character (-1 = empty); saved custom characters (inferred) |
| 0x2EC8 | `item[350]` | bit 0 owned, bit 1 set on first acquisition ("new" is a guess) |
| 0x3028 | `money` | 0..9,999,999 |

Everything else is unidentified.

`flags` at 0x1608:
- bit 0: voice set. Set selects the file base 0xCC32, clear 0x8D4E. It is on by default, so set
  is presumably English in this build (inferred).
- bits 1-2: one per controller, default on; read by battle fighter init. Vibration is a guess.
- bits 3-4: one per controller, default off; meaning unknown.

## A new save (verified)

- Characters locked at the start (ids): 7-10, 0x18, 0x26, 0x36, 0x45-0x48, 0x4B, 0x4C, 0x61,
  0x6E, 0x78, 0x91, 0x95-0xA0. All other ids 0..160 are unlocked.
- Stages locked at the start: 0x11-0x16, 0x1F, 0x20 (of 35).
- Items: every item whose table entry has flag bit 1 is given (table in common file 4).
- Key tables `{2, 1, 0, 3, 4, 5, 6, 7}` for both pads; both volumes 9; flags |= 7.

## Functions

| Name | What it does |
|---|---|
| `Save_Init` | allocates the block, zeroes it, writes defaults |
| `Save_SetDefaults` | writes a new save; also called by the memory card code |
| `Save_UnlockAll` | no callers: sets every character and stage bit, grants items, money 4,850,000 |
| `Save_AddItem(idx)` | marks an item owned and new (name is a guess) |
| `Save_AddMoney(n)` | adds, clamped to 0..9,999,999 (name is a guess) |
| `Save_ResetRules` | restores the six rule defaults (name is a guess) |

## Notes

- About 210 references to the save block in the overlay are not symbolised (they appear as
  `lui 0x30` / `lw -0xD74`), so grepping for the name misses most menu uses.
- `func_002BD950`..`func_002BECC0` in the trailing block of the main executable also grant items
  and money (a reward path); not analysed.
