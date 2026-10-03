# Notes for deterministic netplay

The end goal is a PC port with new online play. This collects what the decompilation has shown
that bears on keeping two machines in sync. Labels as in the README: **verified** means confirmed
by C that compiles to the original bytes; **inferred** means read from disassembly or deduced.

## The simulation step

- Fixed 30 Hz step: every loop ends a frame with a two-vblank wait, the sequence timers count
  `seconds * 30`, and the battle clock advances 100 ms per three ticks. (verified)
- One battle frame is `Battle_Loop`'s body (see systems/battle.md). Input is sampled once, at
  `Pad_Update`; the simulation is the calls from `BtlGame_PreUpdate` to `BtlGame_Update`; drawing
  follows. (verified)
- Pause does not stop the loop. It is a flag each subsystem checks at 126 sites; a paused frame
  still reads the pad and runs the sequence. Both sides must agree on the pause flag. (verified)
- `Battle_Restart` is the state reset for a rematch. (verified)

## What to synchronise

- **Fights do not use the menu input word.** Each fighter builds its own 16-byte `OPRT` record
  (player, stick X/Y, a button word, a command word) from the raw pad through a per-player
  key-config table in the save. (verified, `btl_input.c`)
- **What the game's own replay records is exactly `{buttons, stickX, stickY}` per fighter per
  frame**, taken after key config and double-tap detection and before the ring and the gating.
  The command word is recomputed from the buttons. This is the minimal input a peer needs.
  (recorder and player read from disassembly; the record path around them is verified)
- **The ring never delays.** The 8-entry ring per fighter is pushed and popped in the same call,
  and its public push / pop wrappers have no callers. It is unused plumbing, and the natural
  place to add input delay. (verified)
- **CPU input is injected through fields on the fighter** (`chr+0x1278` on, buttons and stick at
  `chr+0x127C..`). The same path could carry remote input. (verified)
- **Key config is applied before recording**, so two peers with different button layouts still
  exchange the same button word. (verified)
- **Gating is outside the record:** neutral or masked input during the non-fight sequence states
  depends on battle flags and fighter flags, which must therefore be in sync. (verified)
- Menus read `gPad` fields directly from about 110 functions; menu netplay would have to
  synchronise the pad state itself or replace those reads. (verified for the main executable,
  counted from disassembly for the overlay)

## The game already has a replay system

- The battle setup (0x5A8 bytes, tagged "btls" version 7) is copied into a 0x1ABA8-byte block
  that the memory card code saves and loads. (verified code; the names are guesses)
- After the setup come two per-player buffers of 9000 frames each: stick bytes and button
  words, with a count and a position. 9000 frames is 5 minutes at 30 Hz. (read from disassembly)
- So a replay is the setup plus per-frame inputs, re-simulated. The simulation is therefore
  already deterministic enough for the developers' own replays on one machine, and a replay is
  a ready-made desync test for a port: play it back and compare state.
- Not yet known: whether the random seeds are stored with a replay or reset to fixed values at
  battle start. Replays could not work otherwise, so one of the two must happen; finding it
  tells netplay how to seed.

## Sources of randomness

All of these must produce the same values on both machines:

1. `Rand_*`: the broken-refill Mersenne Twister, seeded at boot from a timer through `rand()`.
   263 call sites of `Rand_Range`. (verified)
2. C library `rand()`: used by the battle sequence for voice line choice and for the
   double-KO tie-break, and to seed generator 1. (verified)
3. A second MT19937 copy at 0x252F68, apparently for a scramble or password codec. (inferred)
4. The effect scene's private generator, `(state * 714025 + 4096) % 150889`. (verified code,
   not yet linked)

Seeds have to be exchanged or fixed at match start; the boot-time seed comes from a hardware
timer.

## Things that could desynchronise

- **Direct pad reads in battle-side code.** Two of the three are now known not to matter:
  `DbgCam_Update` (L1 and the sticks) has no callers, and `OrbitCam_Update` belongs to a viewer
  screen and writes only its own camera. `func_001D8590` (pad 0 up/down) is still unexamined.
  (code verified; reachability inferred from the absence of callers)
- **Camera-relative movement comes from the fighter's own camera** (fighter +0x430 / +0x440,
  maintained at 0x1C69C8), not from the camera module, which only copies it out. That code is
  not decompiled and is where to look for camera feedback into the simulation. (inferred)
- **Camera shake advances `rand()`** five times per update while active, from the fighter camera
  update and from the demo camera. It must run the same number of times on both peers,
  including on re-simulated frames. (verified)
- **The demo camera advances inside `BtlCam_UpdateOverride`,** and the battle sequence waits on
  it, so the camera update cannot be skipped on some frames without shifting intro timing.
  (verified)
- **The default view depends on which side is human** (`BtlCam_GetDefaultView`), and that feeds
  an "is this fighter's camera on screen" test used by the effect scene. If two peers set side
  control differently, that test differs; whether it reaches the simulation is not traced.
  (verified code, open consequence)
- **The camera can force a single view** (`BtlCam_UpdateOverride`), which changes what is drawn
  but, as far as the frame loop shows, not what is simulated. (verified in the loop)
- **Loading is asynchronous.** Character and stage loads run as jobs during battle and set
  flags (0x800, 0x1000, 0x2000) that suspend updates. Load times differ between machines, so
  these flags must not gate the simulation differently on each side. (verified)
- **Sound** is queued and sent once per frame and does not feed back into the simulation, with
  two exceptions to check: the per-side voice mute (`func_00259E20`), and the battle sequence
  waiting on `Voice_IsStopped` with a 10 s timeout during intro and win talk. A voice that
  finishes at different times on two machines would advance the sequence at different frames.
  (verified)
- **Frame overruns:** `Vsync_Wait` does not accumulate debt, so a slow frame simply lasts longer;
  there is no frame skipping in the loop. (verified)
- **Floating point:** all game maths is single-precision on the PS2's FPU and vector unit, which
  do not follow IEEE rounding, infinities or NaNs exactly. A port needs both peers to compute
  identically; two PCs running the same build would, but matching the PS2's exact results is a
  separate, harder problem (relevant only for cross-play with real hardware or for replays
  recorded on a PS2).

## Open items

- Decompile the replay recorder and player (`func_001D8388`, `func_001D8470`, `func_001D8330`)
  and find how random seeds are handled across a replay.
- Decompile the fighter camera at 0x1C69C8.
- Match `BtlInput_Update`.
- Find every caller of the four generators inside the simulation.
- The Wii build has online play; its game-side netcode has not been examined yet and would show
  what the developers themselves synchronised.
