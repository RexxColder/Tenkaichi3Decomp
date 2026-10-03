# Roadmap

Agreed order of work (2026-10-03). The goal is a PC port with new online play.

## 1. Finish the fight simulation

- Fighter core (in progress): status, hit handling, collision, members and gauges, action
  queries, flags, movement, the action state machine; the CPU opponent.
- Action handlers and techniques, roughly 0x1E3000..0x212000.
- Effect tasks, 0x12DD80..0x1AE200: the largest block of game code. Effects hit, and hold
  nearly all the `rand()` calls, so they are simulation.
- Stage, 0x23FB20..0x248F28: collision, bounds, destruction, the debris physics caller.
- Consolidation alongside: one unified fighter header in place of the per-file local views;
  real names for placeholder names; C files owning their data.

## 2. Headless simulation on PC (first port milestone)

Run the fight natively with no graphics or sound: stub the hardware calls, feed a battle setup
and an input stream, step it. Validate against the game's own replay format: record a replay
on the original, play it back natively, compare state frame by frame. This checks the decomp's
logic and measures determinism before any netcode exists. Started in parallel with stage 3,
since it needs a platform layer rather than more matching.

## 3. What is seen and heard

- 3D renderer, models, animation (0x102F28..0x11EC10, 0x24BBE8..0x2527B0) and the nine VU1
  microprograms. The PS2 pipeline has to be reimplemented, not translated.
- Asset formats: catalogue the archives; the loaders define the formats.
- HUD (0x2129C8..0x239FF0).

## 4. Everything around the fight

- The menu overlay `DBZP.BIN` (737 functions): character select, story, shop, options.
- Menu support code in the main executable, the memory card module, remaining script users.

## 5. Netplay

Built on a simulation that already runs and can be compared. Look at the Wii build's game-side
netcode at this point to see what the developers themselves synchronised. Requirements are
collected in netplay_notes.md.

## Why this order

The simulation is where a mistake is invisible until two machines disagree. Rendering and menus
are more work in total, but errors there show on screen.

## Added 2026-10-04: required before the headless simulation

The animation player inside the battle object (functions around 0x24D498, 0x24D518, 0x24D610,
0x250570, 0x2505A8, 0x250940, 0x250B88, 0x2500E8; only partly decompiled in btl_obj.c) is
simulation input: fighter and effect code query animation events, hit-event counts and frames,
and model node positions through it every frame. Schedule it with the stage collision queries,
the projectile hit detection (0x1AE200..0x1B4140) and the stage rigid bodies, before the
headless build.
