# Bugs and quirks in the original code

Things a port must either reproduce exactly (when game behaviour depends on them) or fix on
purpose. Each is confirmed by C that compiles to the original bytes unless marked inferred.

## Behaviour-affecting

| Where | What | Consequence |
|---|---|---|
| `Rand_Next` (`src/sys/rand.c`) | The MT19937 refill has only its first loop and the final word; the second loop is missing. | 396 of 624 state words never change after seeding. The game's random sequence is not MT19937. A port must copy the broken refill to reproduce behaviour. |
| `BtlSeq_JudgeByHealth` | On equal health with no rule flag, outside the mode-0 time-up draw, the winner is `rand() & 1`. | A double KO can be decided by a coin flip from the C library generator. |
| `BtlMember_Init` (`battle_setup.c`) | Applies items (and so looks up the default AI type) before storing the character id. | The default AI type is always looked up for character 0. |
| `Dma_PutTexStrips` | The float path adds the X offset without the `<< 4` the integer path applies. | Textured strips whose width does not divide evenly are positioned differently. |
| `Fade_IsDone` | Returns 1 when the done flag is set, which is one frame before the final colour is computed, and also for a slot that is off. | Callers proceed one frame early. |
| `Pad_Update` | A digital-only pad is reset after being read. | Controllers without analog sticks give no input at all. |
| `Pad_Reset` | Does not clear the game-level stick copies. | They keep stale values while a pad is unplugged. |

## Missing checks (crash or corruption if the limit is hit)

| Where | What |
|---|---|
| `BtlPool_Alloc` | No capacity check: an arena that overflows runs into the next one. |
| `BtlPool_CarveArenas` | Slot 5 tests slot 4's mask bit and slot 8 tests slot 7's. Harmless only because every bit is always set. |
| `File_Request` | No check for an empty free list: a 33rd pending request dereferences NULL. |
| `Snd` command queue | A 33rd command in one frame is dropped (the play returns -1); this one is handled. |
| `File_LoadSyncEx`, `Overlay_Load`, boot | Every disc operation retries forever; there is no failure path. |
| `Heap_Free` | A pointer that is not a live block calls an empty report function and then frees a NULL block. |

## Dead or broken code that nothing calls

| Where | What |
|---|---|
| `Quat_Normalize` | Reads the length from its output argument, so it is only correct in place. No callers. |
| `Save_UnlockAll` | Debug "unlock everything". No callers. |
| `Disc_Identify`, `Disc_GetDriveState` | Disc check for the three Tenkaichi games. No callers. |
| `Snd_Term` | Its loop unloads only empty slots, so it does nothing. No callers. |
| `Fade_Init` | Clears only slot 0 of three; the others rely on zeroed memory. |
| `Heap_SetDebug*`, `Dbg_*`, `Gfx_MarkPass`, `Heap_ReportBadFree` | Stripped debug hooks, compiled to empty functions. |
| ~25 list/queue/alignment utilities | Never called: a general utility library was linked in. |
