# Shared brief: effect tasks and stage (seventh batch)

Read docs/agent_rules.md first and follow it exactly, including its last section. Then read
docs/decomp_guide.md, docs/netplay_notes.md, the "Effect scene" section of
docs/systems/battle.md, and docs/systems/combat.md (sections "Hits from blasts and
techniques", "Fighter effect layer", "Character parameters").

Disassembly snapshot to read from (NOT the live asm/ folder, which another process regenerates):
/tmp/claude-1000/-home-z3-Desktop-decomp-bt3/ed9e766e-dea6-4684-86f5-14b7166075c4/scratchpad/snap2/asm/cod/*.s
(find a function with `grep -ln "^glabel NAME$"`; data in .../snap2/asm/data/cod/*.s).
The snapshot predates the newest names: a function shown there as `func_XXXXXXXX` may already
have a name in config/symbols/*.txt. Look addresses up there (`grep -rn "0x001D4F30" config/`)
and use the current name in your C. An integrator is linking other files right now: do not edit
any existing file; declare what you need locally with your own view types.

## Why this code matters

The project's goal is a PC port with deterministic online play. The effect code
(0x12DD80..0x1AE200) is the largest unknown left in the fight simulation: ki blasts, beams and
technique projectiles are effect objects that carry hit records and can hit fighters, so part
of this code IS simulation, and part is purely visual. The stage code (0x23FB20..0x248F28)
provides ground, walls, water, destructible objects and bounds. Separating "affects the fight"
from "only draws" is the most valuable thing your report can do.

## What is already known

- src/battle/btl_scene.c (0x12C9F0..0x12DD80, linked): the effect scene manager `gBtlScene`.
  Frame order `BtlScene_Update`, `BtlScene_PostUpdate`, `BtlScene_CheckStageChange`, then
  `BtlScene_Draw` per view. It owns `BtlScene_Rand` (an LCG, state in the scene, reset at
  battle start) and calls into the code right after it.
- src/battle/btl_pool.c (linked): `BtlPool` arenas used by effect objects.
- src/sys/rigid.c (linked): rigid bodies used for stage debris.
- src/battle/btl_char_coll_b.c, include/battle/btl_char_coll.h: how a fighter resolves a hit
  from an effect hit record (`BtlColl_TryDodge`, `TryDeflect`, `TryReflect`, `TryAbsorb`,
  `TryGuard`, `BtlColl_Hit`), and the layout it assumes for a hit record (0x190 bytes: +0 owner
  object id, +0xC type, +0x20 / +0x30 position / previous position, +0x54, +0x64 source,
  +0x68 definition, +0x180 "seen by the AI").
- src/battle/btl_char_fx*.c, include/battle/btl_char_fx.h: the fighter's effect layer. It calls
  effect modules by address with argument blocks (`FxPosArg`, `FxHitArg`, `FxLineArg`, ...);
  its header lists which request bit calls which address. Those callees are in the effect range.
- src/battle/btl_capi_a.c / btl_capi_b.c, include/battle/btl_capi_*.h: the by-object-id fighter
  API the effect modules call (`BtlCharApi_GetNodePos` has 232 callers here,
  `GetOpponentObjId`, `GetPos`, `GetDir`, `GetHeight`, `IsLockedOn`, `GetDeflectDir`, ...).
- src/battle/btl_tech_b.c: ki blast and technique parameter readers (`BtlKiBlast_*`,
  `BtlSuper_*`): speeds, lifetimes, hit counts, damage of a hit record.
- Random generators (docs/netplay_notes.md): `BtlScene_Rand` (scene LCG), libc `rand()`
  (about 490 call sites, mostly here), the VU0 R register through `Rand_Float01` /
  `Rand_FloatRange` (about 160 sites, mostly here), `BtlChar_RandF` (the fighter generator,
  reached through the fighter API), `Rand_Range` (Mersenne Twister).
- Conventions: +Y is down; 30 frames per second; angles in radians; `Vec4` from
  include/sys/math3d.h. Float literals are truncated by this compiler, so a constant one bit
  off usually means the source used an expression (`BTL_KMH(x) = x * 1000.0f / 3600.0f *
  (1.0f / 30.0f)`, `BTL_DEG(x) = x / 180.0f * 3.14159265f`, also `x * 3.14159265f / 180.0f`,
  `1.0f / 30.0f`, `x / 30.0f`). ALWAYS compare the bits of the .lit4 your object emits with the
  original data: fdiff cannot see constant values.

## What to deliver

Decompile your whole range in address order into the files named in your task. Small functions
first; a function that resists after real effort becomes INCLUDE_ASM with the best attempt in
`#if 0` (and is still described from its disassembly). Effect modules tend to repeat one
shape (create / update / draw / destroy for one effect type, registered in a table): once you
have matched one, the others follow quickly, so find the pattern early. Find and document the
table(s) that register your functions (grep the data files for your addresses).

Naming: every function gets a descriptive name with a prefix for its module (`Eft<Thing>_...`
for effects, `Stg..._` / `BtlStage_...` for the stage). Name an effect for what it is when you
can tell (from who calls it, which fighter request bit or technique starts it, which textures
or models it asks for, its parameters); otherwise name it by structure and mark it `// guess`.

Report, in addition to what agent_rules.md asks:
1. **Simulation or visual.** For each module: does it create, move or destroy hit records, call
   any `BtlColl_*` / `BtlMember_*` / `BtlChar_SetFlag`-style function, write fighter or battle
   object state, or raise battle events? If yes it is simulation: say exactly what it writes.
   If it only builds draw data, say so.
2. **Random draws**: every call, which generator, and whether the result reaches simulation
   state (position, velocity, lifetime, hit timing) or only appearance.
3. **Non-simulation inputs**: camera pose or view, screen mode (split screen), which side is
   human, sound / voice status, loading state, pad, the "is this camera shown" test, frame
   time. Say whether the dependency reaches simulation state.
4. **Order dependence**: iteration order over objects or fighters where the order changes the
   outcome; anything keyed on object id 0 / player 0.
5. Object layouts (size, fields, list links), the update / draw entry points and who calls
   them, and emitted data (jump tables, .lit4 with addresses, strings, file-scope tables).
