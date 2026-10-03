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
  key-config table in the save. (inferred; being decompiled as `btl_input.c`)
- That record passes through an existing 8-entry ring per fighter, normally with zero delay, and
  there is an existing path that takes input from fields on the fighter instead of the pad.
  These are the natural hooks for input delay and for injecting remote input. (inferred)
- Menus read `gPad` fields directly from about 110 functions; menu netplay would have to
  synchronise the pad state itself or replace those reads. (verified for the main executable,
  counted from disassembly for the overlay)

## The game already has a replay system

- The battle setup (0x5A8 bytes, tagged "btls" version 7) is copied into a 0x1ABA8-byte block
  that the memory card code saves and loads, and input, camera and HUD code test whether it is
  active. (code verified; "replay" meaning inferred)
- If replays re-run the simulation from the setup plus recorded inputs, the simulation is
  already deterministic enough for that, and a replay is a ready-made desync test: play it back
  on the port and compare state. How inputs are stored in the block is not decompiled yet.

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

- **Direct pad reads in battle code**, bypassing the input record: `func_0023F0F0` (L1 and the
  sticks; looks like manual camera control), `func_0023F708` (sticks), `func_001D8590` (pad 0
  up/down). If any of these feed the simulation, their input must be synchronised too. Fighter
  movement in this game is camera-relative, so a locally controlled camera is a real risk.
  (inferred; the camera module is being decompiled)
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

- Decompile and verify the fighter input path, including who sets the delay and the injection
  flag.
- Establish what the replay block records and how playback feeds the fighters.
- Establish whether the pad-reading camera code affects the simulation.
- Find every caller of the four generators inside the simulation.
- The Wii build has online play; its game-side netcode has not been examined yet and would show
  what the developers themselves synchronised.
