# Wind Waker HD's sky and light, measured against BlueWake (Oct 30, 2026)

Research notes from comparing Cemu (Wind Waker HD, USA) with BlueWake's HD look on Windows (i5-12600KF,
RTX 5070 Ti). The Cemu tools and captures named here are local to that PC and aren't in the repository. HD's data was changed in Cemu's memory only; nothing was saved. Everything here is
measured, unless marked as a guess.

## Setup

- **Cemu 2.6, portable:** `C:\src\tools\cemu\Cemu_2.6`. Settings, packs, the HD save and the log are in `portable\`.
  - **Packs:** stock HD shadows. The Ultra 300% Shadow Resolution pack that came from Arch was removed, because we compare against the original game (backup `settings.xml.with-ultra-shadows`). Still on: Resolution 5160x2160 (21:9), Mega Cheats, Stage Select (BlueWake, L + R + D-Down).
  - **Volume:** TVVolume 0.
- **Tools:** `C:\src\tools\cemu\tour-win\`.
  - `vpad_win.py` (the server) and `pad.py` (the client): a virtual Xbox pad on ViGEmBus. It presses only while Cemu or BlueWake is the foreground window. Before that guard, presses leaked to the desktop and opened the Start menu and the on-screen keyboard.
  - `step.py` and `bwstep.py`: bring the game to the front, send pad commands, take a shot.
  - `f12shot.py`: Cemu's own F12 screenshot. PrintWindow comes out grey because of Vulkan.
  - `cemu_mem.py`: reads and sets HD's clock, `*(0x101F84DC)+0x44`, 15 degrees an hour.
  - `cemu_virt.py`: finds and edits HD's sea Virt (sky) and Pale (`PALE=1`, light) tables in RAM. `restore-all` writes a whole table back from the disc.
  - `wrap_falloff.py`: measures the sky wrap.
- **Matched saves at 10:00 on Outset's bridge** (the default load spawn at this point, with the lookout behind):
  - **Cemu:** quest log 1, "d", saved just after Aryll woke Link.
  - **BlueWake:** quest log 3 on `C:\src\test-builds\hd-fog\cirrus-ql3.card`, a copy of the tour card. A new game through the stage select's Cutscenes > New opening and Aryll's talk, then re-saved after a warp to `sea:44:0` at 10:00.
- **Holding Link still:** crouch (ZR), crawl a moment, keep crouching. About 99% still; one lock of hair moves in the wind.
- **BlueWake runs:** `C:\src\test-builds\hd-fog\run-load.ps1` loads quest log 3 with no warp. Variants:
  - `full`: HD sky models and linear blend on.
  - `noshadow`
  - `kumo`: test pack with the cloud colour set white.
  - `log`: fog log.

  `run-outset.ps1` warps instead, and `run-windfall.ps1` is for the Windfall dock.

## Starting time

- **GameCube:** a new game starts its clock at 165 degrees, 11:00 (`d_save.cpp:54`).
- **HD:** a fresh new game in Cemu read 10:00 (150 degrees) from 10 s after Start through the legend and into the opening. So HD starts an hour earlier.
- **The developers' stage select:** its "Norm" time writes 120 degrees, 08:00 (`d_s_menu.cpp:254`). A log started through the stage select keeps that, because time stands still on Outset.
- **Left for later:** whether BlueWake's HD look should start new games at 10:00.

## The sky palette (Virt): HD's five colours and five floats

- **HD's entry:** each Virt entry is 0x38 bytes, the GameCube's 0x24 plus five floats. The five colours are:
  - kumo, the cloud layers
  - kumo centre, the puffs
  - sky, the dome
  - uso umi, the horizon band ("false sea")
  - kasumi mae, the low front haze
- **The floats scale those colours,** one per colour, in that order. Tested from 0 to 4 at Outset 10:00, entry 21:
  - **0:** the cloud layers, dark at 0 and white at 4.
  - **1:** the puffs. The disc value is 1.1 here.
  - **2:** the dome. At 0 it goes nearly black. It does not light the ground; what moved on the ground was cloud shadows.
  - **3:** the horizon band.
  - **4:** the front haze strip right on the horizon, between the cliffs. No visible change at 0 or 4 from either view.
- **Values:** nearly all are 1.0 in the sea's 37 entries. Only entry 21 (1.1 for the puffs) and entry 23 (1.636 for the horizon band) differ.
- **Cloud colour:** HD multiplies the cirrus by the kumo colour; set to red, the cirrus turned red. The alpha (180 in entry 21) did nothing visible.
- **BlueWake's cirrus at 10:00:** greenish grey, because HD's kumo colour for this hour is mint (187, 255, 170 linear, which is 221, 255, 212 after our gamma).
  - Setting it white over-corrects toward pink. Mid-cloud band in R, G, B: Cemu 191, 245, 245; mint 167, 215, 201; white 184, 214, 226.
  - HD lands between them, and brighter. The clear sky behind is also darker and less cyan in BlueWake: 71, 178, 222 against 113, 205, 237. The two sky switches below help.
- **Two HD sky switches are off by default:** "HD's taller haze and cloud layers" (`BLUEWAKE_HD_SKY_MODELS`) and the linear blend (`BLUEWAKE_HD_SKY_LINEAR`).
  - Without them BlueWake's horizon stays deep blue.
  - Every comparison shot before the `full` runs lacked them, including the Windfall tower numbers below.

## The sky wrap: the haze on the far side of the island

The effect Dale saw in Cemu: the edges of scenery seen against the sky take the colour of the sky behind them.

- **Where:** on the cliff edges by a gap, the far rope bridge and the far rock stacks.
- **It's a mix, not an add.** On the rock, green fell from 115 to 100 as red rose.
- **It follows the hour:**
  - 06:30–07:00: dim lavender, nearly invisible.
  - 07:30: pale.
  - 10:00: cyan.
  - 18:30: orange.
- **It's the same with stock and Ultra shadows,** so it isn't a cover for shadow acne.
- **It reaches far scenery and works in every direction,** so it isn't BlueWake's edge light.
  - BlueWake's edge light (RecompCore `post_fx.cpp`, `kEdgeShader`) is a fixed warm colour (1.0, 0.95, 0.82) x 0.4, toward the sun only, 3 steps of 1.5 px, faded out by 1,500 units, and off when the sun is below about 3 degrees.
- **Link is not lit by any of the five sky colours.** With all five red, his hair and shirt didn't change.
- **Falloff:** the share of the sky colour mixed in, by distance from the sky (per 1080 lines, red-dome pairs at stock shadows):

  | Distance | 1–2 | 3 | 5 | 7 | 10 | 14 | 20 | 28 | 40 | 56 |
  |---|---|---|---|---|---|---|---|---|---|---|
  | Share | .34 | .26 | .23 | .20 | .17 | .12 | .09 | .06 | .035 | .016 |

  - **Fit:** two Gaussian blurs of the sky mask, sigma 4 px and 25 px per 1080 lines, mixed at 0.32 and 0.46 (rms error 0.008).
  - **Not motion:** a repeat with Link still gave 0.41 at 0–4 px, 0.23 at 10–20 and 0.04 at 40–80 (at 2160 lines). Motion-only pairs gave 0.
- **Implementation plan:** a new pass in RecompCore's post effects. Branches are made: RecompCore `codex/hd-sky-wrap` from 259627f (in `C:\src\bluewake\ref\recompcore`), and BlueWake `codex/hd-sky-wrap` from 2b63027 (the main checkout).
  - **Sources:** pixels with no depth (depth 0) whose view ray is above the horizon. Excluding below the horizon keeps the far sea out.
  - **Masks:** premultiplied colour and mask at half size, blurred twice (sigma 4 and 25 per 1080 lines).
  - **Composite:** a mix into pixels with depth, after the exposure and before bloom. Outdoors only, off by default, with its own switch under the HD look.
  - **Open:** the islands' far models also have no depth, so they'd count as sky.

## The light palette (Pale) and Link

- **Entries:** 0x6C bytes, the GameCube's 0x2C plus 16 floats. The first float isn't a float (garbage values). Outset at 10:00 blends entries 1 and 2.
- **What each colour does**, set red one at a time with Link still:
  - **actor_c0:** the actor ambient (`d_kankyo.cpp:3088`, `addcol_amb`). It's not AO.
  - **actor_k0:** the actor diffuse, the sun's colour (`addcol_dif`, `:3122`).
  - **bg0_c0:** the cliffs' shade.
  - **bg0_k0:** the sunlit pier and cliff faces.
  - **bg1_c0:** the sea foam around the dock posts (Dale).
  - **bg1_k0:** the sea.
  - **fog:** the mid-distance cliffs and houses. HD's fog is in this palette, as on the GameCube.
  - **bg2:** unclear from the pier; black against white changed only moving things (palms, wind streaks, foam). It may be foliage or a night-time effect (Dale): come back to it at night, or from a spot with still plants.
  - **bg3:** the houses' lit windows, a night light. At 21:00 (bg3 set in every entry) the windows of both houses go dark at black, white at white and red at red; normally they're orange. At 10:00 its value is 0, 0, 0, and only bg3_c0 showed (the lookout's window, set red).
  - **bg2 at 21:00,** set in every entry to black, white and red: no visible change on Outset from the pier. It isn't used here, or only by models out of this view.
  - **Testing with the screen locked:** Cemu can't be brought to the front, and F12 can't reach it, so the shots fail. With the monitor turned off and the session unlocked, they work.
- **The 16 values after the GameCube's 0x2C bytes** (tested at 10:00, entries 1 and 2, each at 0 and 4):
  - **The first isn't a float. It's a twelfth colour, the shadow colour** (RGB plus a padding byte):
    - morning (entry 1): 73, 95, 134
    - noon (entry 2): 27, 98, 181
    - night (entry 8): 75, 75, 75

    Set red, Link's shadow on the pier, the cliffs' shaded sides and Link's shaded side all turned red. Black darkens them, white makes them pale.
    BlueWake uses one fixed shadow colour, (0.22, 0.41, 0.60) (`post_fx.cpp:55`, from Link's shadow on Windfall's dock at noon), at every hour. HD's changes with the hour. The host could pass it to the shadow pass each frame, blended between entries like the other colours.
  - **Floats 1–11 are brightness multipliers for the eleven colours, in order:**

    | Float | Colour | What it changed at 4 |
    |---|---|---|
    | 1 | actor_c0 | Link, strongly |
    | 2 | actor_k0 | Link, slightly (2.0 in the morning, 2.75 at noon) |
    | 3 | bg0_c0 | the cliffs' shade |
    | 4 | bg0_k0 | the pier and the sunlit cliffs (2.0 in the morning, 3.0 at noon) |
    | 5 | bg1_c0 | nothing visible |
    | 6 | bg1_k0 | the sea |
    | 7–10 | bg2, bg3 | nothing at 10:00 |
    | 11 | fog | everything lighter |
  - **Floats 12–15:** no clear change. Possibly unused.
- **Red is a bad probe for Link,** because his colour is the texture times the light. Black works on any texture:
  - **k0 black (ambient only):** the lit parts go black: the back of his shirt, the elbow and shoulder, the bright side of his knee. The shaded side keeps its colour: the back of his head, his bottom, the soles, the boot.
  - **c0 black (sunlight only):** the shaded side only darkens somewhat, lime to darker green on his hair. So HD's shaded side gets the ambient plus a good share of soft sunlight. It's a gradient, not the GameCube's hard two-tone.
- **The sheen on Link's hair:** HD's lavender actor ambient at 10:00 (entry 1, 186, 180, 205 after gamma) over the yellow hair, plus that soft sunlight. It reads as a cool lime. It's not a sky reflection.
- **BlueWake at 10:00, the same view:** the back of Link's head is dark olive with a dithered edge from the sun shadow map. With sun shadows off it's flat GameCube yellow. HD: bright, with a soft lime gradient and a paler outline.
- **Two things to tune:**
  1. A softer light-to-shade transition on characters, with sunlight wrapping past the edge.
  2. The shadow map not double-darkening a character's own shaded side.
- **Actor colours, HD (after gamma) against GameCube:**

  | Pale | Hour (guess) | GameCube shade (c0) | HD shade (c0) | GameCube light (k0) | HD light (k0) | HD floats |
  |---|---|---|---|---|---|---|
  | 1 | morning | 180, 174, 199 | 186, 180, 205 | 247, 238, 229 | 247, 238, 223 | 2.0, 2.0 |
  | 2 | noon | 156, 140, 134 | 185, 171, 146 | 255, 255, 255 | 255, 248, 210 | 2.75, 3.0 |
  | 3 | evening | 172, 133, 97 | 145, 132, 130 | 215, 188, 105 | 241, 200, 136 | 2.0, 2.0 |
  | 4 | dusk | 116, 108, 124 | 130, 106, 102 | 131, 129, 145 | 130, 149, 140 | 1.0 |
  | 5 | night | 81, 100, 148 | 96, 150, 151 | 119, 148, 176 | 119, 167, 191 | 1.0 |

## Windfall's dock tower (parked)

- **Too light under HD's sky:** at noon the upper wall measured 135 with HD's sky, 62 with the GameCube's, and about 80 in Cemu. That was measured without the two sky switches, so it needs redoing with them on. The textures aren't the cause (HD's Windfall art averages the same as the GameCube's).
- **A lit gap under the balcony:** the slope bias. Far slope must stay 20 because of Ice Ring Isle; `STEPS=1` doesn't help. Idea: let the gap fill's 120-unit limit grow with the reach, and look again at its fade (`post_fx.cpp` gap fill).

## Open questions and next tests

- **The light palette's 16 floats:** which float scales which colour (at noon two are 2.75 and 3.0).
- **bg2:** nothing on Outset at 10:00 or 21:00. Look for it on another stage.
- **The soft-sunlight share on Link's shaded side:** divide the c0-black shot by the normal one on the shaded side.
- **HD's SSAO's part in Link's softness:** Cemu's `WindWakerHD_NoSSAO` pack, after a restart.
- **The BlueWake shadow map's debug overlay on Link** on the bridge (is he shadowing himself?).
- **Dawn and dusk comparisons** of Link and the cirrus colour, to fit how HD applies the kumo colour and the 1.1.
- **The cirrus ring's position:** a longer timed run in both games (start angle or speed).
- **HD's puffs are larger** and keep filling the sky. BlueWake's drift away.
- **Whether the two sky switches should come on with HD's sky.**
- **New games starting at 10:00** under the HD look.

## Captures

- `C:\src\tools\cemu\tour-win\shots\`:
  - `cir-*`: Cemu at 10:00 after the load.
  - `fx*`, `floats-min-max.png`, `floats-heat.png`, `floats-bridge.png`, `sk-*`: the sky floats.
  - `rd-*`, `rd-redmaps.png`, `gap-edge-closeup.png`: the red dome and the wrap.
  - `tod*`, `tod2-sheet.png`: times of day.
  - `lc-*`, `pa-*`, `pb-*`, `pk-*`: Link with the sky and light colours.
  - `stock-*`, `ultra-*`, `planks-stock-ultra.png`: stock and Ultra shadows.
- `C:\src\test-builds\hd-fog\`:
  - `outset-*`: BlueWake runs.
  - `cirrus-*.png`, `link-hair-*.png`: comparisons.
  - `windfall-*`, `tower-*.png`, `awning-*.png`: the tower.
  - `pack-kumo`: a hard-linked pack copy.
  - `arch-shots`: copies from the Arch disk.
