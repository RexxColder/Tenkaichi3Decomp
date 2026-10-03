# Open questions

## Build and layout

- The C files reference their global variables as `extern` from assembly; none defines its own
  data yet.
- Original source-file boundaries are mostly unknown. Evidence so far: `.rodata` alignment (the
  object containing the battle sequence starts at 0x215540), and delay-slot behaviour that only
  matches when certain accessors are not defined earlier in the same file (`battle_work.c` vs
  `battle_load.c`).
- The CPU player's code is two objects. The second one's file-scope tables (0x2EDA70..0x2EDF08)
  sit between the two groups of function-local data, which puts the boundary after
  `BtlAiStep_Unk23` (ends 0x1B6B08) and before `BtlAiCond_React` (0x1B7188). Nothing narrows it
  further: the functions in between emit no read-only data and link identically on either side.
  The files are split at 0x1B6D50: `BtlAi_ScaleByLevel` (0x1B6D00) is on the first object's side,
  because two of its callers in the second object (`AiThink_GetSubRate4`,
  `AiThCond_ActRateByFlags`) only match with it outside their file. The first object is
  taken to start at 0x1B4140 (its first read-only item is the jump table at 0x2ED8C0); the data
  would equally allow an earlier start. The second object runs to 0x1BB128 at least
  (`AiThink_FindWeightColumn` needs the tables defined in its file, and the read-only data is
  continuous to 0x2EE248); whether `btl_ai_mgr.c` (from 0x1BB128) is a third object is not known.
- Other object boundaries placed by the same kind of evidence (a function that only matches with
  a callee defined, or not defined, earlier in its file): `btl_char_hit.c` runs to 0x1CA6D0;
  `btl_char_coll_b.c` starts somewhere in 0x1CA6D0..0x1CAEF0; `BtlColl_NextPoolMember` (0x1CDCA8)
  is in the same object as `BtlMember_Damage`, and the file split at 0x1CDCA8 is only the latest
  place the boundary can be (coll_b and member may be one object); `btl_input.c` runs to 0x1D60A0.
- More boundaries from the same evidence (action-handler batch): the effect request-bit helpers at
  0x1CF578 are in one object with the effect requests up to 0x1D3B40 at least (`BtlFx_UpdateGroundFx`
  needs `BtlChar_IsFxBitNew` above it), and the read-only data says that object does not start later than
  the member code's tables end, so `btl_char_member.c` and `btl_char_fx.c` may be one object;
  0x1E3158..0x1EA5F8 is one object (`BtlAct_AttackDashHandler` needs `BtlAct_RequestAttackEnd`), and
  continues `btl_char_action.c` by its data; 0x1F5460..0x1FC2B0 is one object
  (`BtlAct_SuperRushDashHandler` only matches in a file with the functions before it; which one it needs
  is not known); 0x1FC598..0x203168 is one object (`BtlAct_SwitchArriveLand`, `BtlAct_KoSwitchFlyIn` need
  `BtlActChange_SetFlags` / `BtlActChange_Finish`), and `BtlAct_Request` (0x1E0290) is not in it.
- Effect and stage batch: 0x13C300..0x13F430 is one object (`EftBurst_Update` needs the particle initialisers
  above it; the two caller-less tests at its end are its non-static inlines); 0x140338..0x147050 is one object
  (`EftWaterRing_Update` needs `EftWater_GetSurfaceY`), and nothing says where it starts (the steam emitters at
  0x13F430 are linked in the same file); 0x15AB38..0x15C728 is in one object with what precedes it
  (`EftRushShot_UpdateAttached` needs `EftRushShot_UpdateModels`). Against a merge: `EftBlast_PostUpdate`
  (eft_j.c) stops matching when eft_i.c is in front of it in one file, so there is a boundary in
  0x1532A0..0x155588 or before. Not tested: eft_c + eft_d, eft_g + eft_h + eft_i, eft_l_d + eft_m, the stage files.
- `BtlAct_SuperRushFollowHandler` (btl_act_f.c): whether its jump-table dispatch comes out in the
  original form depends on unrelated declarations earlier in the translation unit; it matches with a
  redundant prototype in front of it. What state of the compiler decides it is not understood.
- The 42 functions at 0x2BD230..0x2BF6B0, after the libraries, are game code (memory-card menu
  UI) that never uses `$gp`. Why they sit there, and whether they were built with different
  flags, is unknown.
- Two functions in the main body reach short `.sdata` strings with absolute addressing
  (0x1094A8..0x111358), suggesting objects built with different small-data flags.
- Assembler prelude gaps (see decomp_guide.md): a single-instruction load, store or `la` between
  a compiler-filled delay slot and an unfilled branch; `li.s` under `-G0`. (The `symbol(reg)`
  load in front of a return turned out not to be a gap: it depends on where the table is defined.)

## Functions that resist matching

Still pulled from assembly inside linked files:

| Function | File | What differs |
|---|---|---|
| `BtlInput_Update` | `btl_input.c` | 6 of 149 instructions: the emission order of twelve stores |
| `DemoCam_Update` | `btl_demo_cam.c` | 11 of 384: two saved registers swapped in one branch |
| `BtlText_DrawScrollBar` | `btl_seq.c` | one register swap |
| `Ot_Reset` | `gfx_ot.c` | the original copies an address through two extra registers |
| `Vu1Node_Animate` | `vu1_packet.c` | one instruction short; different loop induction variables |
| `BtlScene_TestCharPackBit`, `BtlScene_IsEffectHidden` | `btl_scene.c` | a saved-register choice; a switch's shared block |
| `IopHeap_PrintFree` | `iop_heap.c` | the original saves an unused register and does not tail-call |
| `Movie_ReadBuf` | `movie.c` | 5 of 44: order and register choice |
| `Sprite_DrawPicture` | `sprite.c` | 12 of 150: scheduling in the row loop |
| `Rigid_Init` | `rigid.c` | 2 of 72: two instructions swapped before a memset |
| `PadWatch_GetMissing` | `pad_watch.c` | 2 of 69: delay-slot fill |
| `ChrCam_CalcCut` | `btl_char_cam_cut.c` | 248 of 565: register allocation in the four "resolve a node" blocks; it owns two `.lit4` constants, which is why the fighter camera is three files |
| `BtlAiStep_Unk17` | `btl_ai_seq.c` | 63 of 154: branch layout of the first half, registers of the class comparisons |
| `BtlAiCond_TypeRateByOppAction` | `btl_ai_cond.c` | 5 of 50: a result register and the place of two pointer adds |
| `BtlAi_GetPairRate`, `BtlAi_GetQuadRate` | `btl_ai_cond.c` | 75 of 90: the original repeats the row code in every even case and shares one tail for the odd ones |
| `AiThink_TestSkill` | `btl_ai_cond.c` | about 260 of 308: eight strength-reduced pointers in the slot loop, four of them on the stack |
| `AiThink_BuildTotals` | `btl_ai_cond.c` | 12 of 80: where the pointer to the last table is formed |
| `AiThink_GetRollRange` | `btl_ai_cond.c` | 25 of 64: registers and the order of two byte loads |
| `AiThink_RollGroupGate` | `btl_ai_cond.c` | 50 of 112: a register in the first switch, a shared argument load in the second |
| `AiThink_EvalRules` | `btl_ai_cond.c` | 83 of 270: register choices that follow from one (a1 kept free) |
| `AiThink_GetBlastStep` | `btl_ai_cond.c` | 11 of 32: registers only |
| `BtlAiSense_IsBusy` | `btl_ai_act.c` | 8 of 48: a delay-slot fill |
| `BtlFx_SpawnSpeedLines` | `btl_char_fx.c` | 35 of 112: the store order and constant registers of the block that fills the 0x60-byte parameter structure |
| `BtlFx_SpawnDamageSparks` | `btl_char_fx.c` | register allocation only (two saved registers swapped, one value spilled); owns the jump table at 0x2EF020 |
| `BtlActB_TickMemberChange` | `btl_act_a.c` | 14 of 42: the last test compiles to slti / sltiu and the branch layout follows from it |
| `BtlAct_GuardHandler` | `btl_act_c.c` | 2 of 298: the order of two argument loads in front of one call |
| `BtlAct_GrabDash` | `btl_act_h.c` | 18 of 146: the original keeps a branch where this compiler makes a conditional move |
| `BtlCharApi_HasKiBlastType2`, `BtlCharApi_HasKiBlastType3` | `btl_capi_a.c` | same instructions, different block layout of the search loop |
| `EftHit_SetTaskFlag`, `EftHit_IsStoppedByHit`, `EftHit_ClashTech`, `EftHit_InitMultiHit` | `eft_a.c` | 13 of 29 (order of the by-value vector copy and of the list load); 14 of 24 (two branch-likely kind tests that every C form merges); one instruction (`a` copied to a2 on entry); 3 of 20 (which of v0 / v1 holds the definition) |
| `EftGfx_LightClutSpecular`, `EftGfx_DrawPolyAvgZ`, `EftGfx_DrawPolyFixedZ`, `EftGfx_DrawPolyAvgZFront`, `EftGfx_DrawPolyScaledZ`, `EftGfx_DrawSprite` | `eft_a.c` | 22 of 198, registers only; the polygon functions walk a pointer to `scr[i - 1][2]` in the original and to `scr[i][2]` here (7 of 115, 20 of 122), plus register allocation and the order of the clamps |
| `EftPrim_DrawBillboard`, `EftPrim_DrawQuadDepthScaled`, `EftPrim_DrawTriangle` | `eft_b.c` | about 50 of 370 (registers and scheduling of the corners); 8 of 200 (f29 / f30 swapped); 12 of 326 (scheduling of two constant loads) |
| `EftBubble_EmitBody`, `EftBubble_Add`, `EftGeyser_DrawColumn` | `eft_b.c` | the original does not strength-reduce the bone table index (owns the table at 0x2EC620); 5 of 151 (one store before instead of after the count increment); register allocation throughout (owns the table at 0x2EC6E0) |
| `EftGeyser_StartSmoke`, `EftGeyser_StartSteam` | `eft_c.c` | 17 of 162 and 19 of 126: the order of the stores that fill the emitter description |
| `EftSurf_DrawPolyOtClipped`, `EftSurf_DrawTriOt`, `EftSurf_DrawTriDirect` | `eft_d.c` | 458 instructions against 447; two too long; two short (the original keeps a value on the stack) |
| `EftBurst_DrawQuadRot`, `EftBurst_DrawModel` | `eft_d_b.c` | 22 of 274, all in the prologue (how the two 0x40-byte tables are copied); 561 instructions against 600 |
| `EftWater_AddSplashFor` | `eft_e.c` | 4 of 110: the square root's argument lives in f12 in the original and f1 here |
| `EftWater_IsOnScreen`, `EftWater_DrawBillboard`, `EftWater_DrawSprayQuad`, `EftWater_DrawClippedFan` | `eft_e.c` (second part) | inverse branch layout at the end of each case; 23 of 377 (registers of three constants); 126 of 456 (the roll loop counts up in the original); registers almost everywhere |
| `EftUtil_DrawTri`, `EftStorm_DrawBolts`, `EftStorm_SpawnBolt`, `EftStorm_DrawRainLines` | `eft_g.c` | two instructions swapped; 506 instructions against 636 (the original writes the second sprite twice); three instructions (a branch around `kind = 1`); slt / movn against slt / movz |
| `EftSmoke_Update`, `EftSmoke_Draw`, `EftBound_BuildWall`, `EftBound_Init` | `eft_g.c` | where the flag word is reloaded; two more float copies and a larger frame in the original; numbering of the saved registers; one store's position |
| `EftShot_BuildParam`, `EftShot_GetLeadTime`, `EftVolley_Init` | `eft_h.c` | 499 instructions against 502 (address arithmetic shared differently); the original loads each constant into f1 and copies it to f0; 6 of 72 (a repeated load of `slot->param`) |
| `EftEmit_SpawnType0`, `EftEmit_SpawnType16`, `EftEmit_SpawnType14`, `EftEmit_SpawnType5` | `eft_h.c` | saved-register numbering and a shared tail; 4 of 195 and 4 of 213 (s1 / s2 swapped); 16 of 191 (registers of the resource address) |
| `EftEmit_SpawnType9`, `EftEmit_SpawnType10`, `EftEmit_SpawnType15`, `EftEmit_SpawnType12`, `EftEmit_Spawn` | `eft_i.c` | 17 of 195, 25 of 207, 52 of 229, 56 of 205: where memset's arguments are set up in the block that fills the argument, and swapped saved registers; `EftEmit_Spawn` 66 of 753 (owns the two jump tables at 0x2EC990 / 0x2EC9B0) |
| `EftSweep_AddMark`, `EftSweep_UpdateMarks` | `eft_i.c` | a constant the original keeps inside the loop is hoisted here (1, and 1.0f) |
| `EftBlast_Init` | `eft_j.c` | 9 of 104: registers of three loads that gcse makes one pseudo; matches with `-fno-gcse` |
| `StgFrustum_Build`, `Stg_FadeByCamDist` | `stg_a.c` | 8 of 164 (one float register swap); 4 of 54 (f0 / f2 swapped) |
| `StgPart_Animate`, `BtlStage_BreakObj`, `BtlStage_UpdateObjs` | `stg_a_b.c` | 29 of 288 (registers in the interpolation block); the shake section (the original keeps 1500 in a saved register); the original runs out of saved registers and spills |
| `ScrXfade_StoreHalf`, `ScrXfade_Draw`, `ScrWarp_Draw`, `StgFog_Draw` | `stg_b.c` | 4 of 121 (scheduling before the first call); 25 of 145 (0x700 hoisted into a register in the original); 148 instructions against 150; 156 of 229 (schedule of six constants) |
| `StgHaze_Draw`, `StgBlur_Draw` | `stg_c.c` | GS packet loops: a second copy of `rows - 1` and reloads from the stack in the original |

`StgVu_RotateZ`, `StgVu_RotateX`, `StgVu_RotateY` (0x240C68..0x240DB8) are hand-written VU0 macro code and stay an
assembly chunk between `stg_a.c` and `stg_a_b.c` (they cannot be INCLUDE_ASM, see decomp_guide.md).

## Game structure

- What each of the 70 `gProgress->mode` values (overlay dispatch) is.
- Which game modes battle modes 2..5 correspond to in the menus; where modes 8 and 9 come from.
- The meaning of most battle event ids, of "character flag 7" and character flag 0x128.
- The save block's `slot[9]`, `unlockFlags`, `rule[6]`, and its large unidentified ranges.
- What sets the per-side voice mute (`func_00259E20`).
- What the three unanalysed boot inits do (`func_00116BA8`, `func_00239FF0`, `func_0023D0E0`).
- Whether further code is ever loaded from the archives (only one overlay table entry exists).

## Not started

- The stage, effects, HUD and camera internals.
- The 3D renderer, the model/texture/animation formats and the nine VU1 microprograms.
- The VU0 vector library (about 223 functions).
- The memory card module and the movie player.
- The whole menu overlay (737 functions).
- `SOUNDS.IRX` (the sound driver) and the bank format.
- The archives: nothing has been extracted or catalogued.
- The Wii build's netcode.
