# birdview

Status: IN PROGRESS
Updated: 2026-11-24 07:45
Branch: lane/birdview (see `git log -1 lane/birdview`)

The user picked this from a list, in the marshal's words: "Bring the camera in or enlarge the birds so the new
mangrove wildlife is actually visible from the boat." The goal, the deliverables and criteria 1 to 8 below are the
marshal's reading of that, not the user's words. Criterion 9 (pelicans) came during the work. The user's words were
"use this as a model for pelicans" (with a photograph, `KeysRun-devkit/refs/pelican_ref.png`), "and this one for
others, mix them up" (a second photograph that reached this lane only as the marshal's description), and the
standing rule "i just want to keep it realistic". How those were read (the American white pelican in place of the
photograph's great white pelican, the brown pelican staying the common one, the two kinds together where pelicans
gather) is the marshal's reading, and this lane followed it. What the lane itself had to guess is under "Guesses".

Evidence is outside the repo in `/Users/mat/Documents/Unreal Projects/KeysRun-devkit/lanes/KeysRun-birdview/`:
`shots/<tag>/KRScript_NN.png` (and `KREco_*.png`), `shots/sheets/*.png` and `log_<tag>.log`. Every run had request
R1 (the hub patch) applied locally, and the runs of 24 November also R4; neither is committed. The save's boat is
the 43 ft one. No `verifier` agent type was available: the lane followed the verifier's brief itself (every shot
cited was opened; what was not checked is said).

**Two things to know about the evidence.**
- The lane was worked in two sittings by two agents. What sections 3 and 6 and the `c3a`, `rook2`, `fwat` shots
  show is as the first sitting recorded it; the second sitting opened the sheets and the `pk*` shots, not those.
   Runs tagged `m_*`, `tour*`, `c3a`, `rook*`, `fwat*`, `fbirds`,
  `else_*`, `pf_*` are of 23 November, on builds before the pelican rebuild and the heron colours; runs `pk*`,
  `e2_*`, `pf2_*` are of 24 November on the final code (bar a last colour-only change, said where it matters).
- The game slot is shared, the lock is not first come first served, and during the night of 23 to 24 November the
  user's own copy of the game and another lane's compiler were running: three runs of this lane were spoiled by it
  (`pk4`: the game stalled for seconds a frame; `pk6`: an Esc from the keyboard paused it; `e2_before`: commands
  bunched at start-up). They are not cited. Nothing on frame time was measured in that state.

## Acceptance criteria

- [x] 1. Measure first - table and reasoning below (`log_m_before.log`, shots `m_before/50`, `m_before/51`).
- [ ] 2. Every kind can be seen in normal play - 12 of 13 kinds shown at or over their pixel height in the player's
  view, with a sheet of crops (`shots/sheets/kinds_final.png`, run `pk5`; kingfisher and anhinga from `fbirds`);
  the osprey is NOT shown near (11 px, perched 124 m off); the 60 m shore is shown but against the light.
- [x] 3. A closer look, for the player - `c3a` (below). The "released" line when a line goes out was fixed in code
  after that run and has not been re-run.
- [ ] 4. Life in and on the water reads too - a school's dark patch, a mullet's jump, a shower and jacks' white
  water are shown (`fwat`, `fwat2`, `shots/sheets/water.png`); crabs are shown from 11 m, NOT from 8 m.
- [ ] 5. The faults left behind - snag, bookmarks and fiddlers shown; ibis, kingfisher and night heron shown from
  one side only; the pale wash on dark birds in full sun is NOT cured (see "Not right yet").
- [x] 6. Rookeries are a sight - `rook2` (below).
- [ ] 7. No slowdown - see section 7 for what was measured and what it is worth.
- [x] 8. Nothing else moves - `e2_on`, `e2_classic`, `else_before` (below); the "before" half of the reef pair is
  from 23 November's build.
- [x] 9. Pelicans on the user's pictures - shots below; one shortfall: the brown pelican washes pale in low sun.

## 9. Pelicans (added by the user during the work)

What is in the game now (`EmitPelican` in `KeysMangroveEcoBirds.cpp`; `KeysMangroveLife::Pelican` for the still ones
on snags and dock piles):
- **One model, three plumages.** Pouched bill with a hooked nail, a big head, the neck tucked on the shoulders in
  flight, two-panel broad wings (coverts along the leading edge, flight feathers behind, five broad primaries
  spread at the tip), a folded wing at rest whose dark tip hangs past the tail, webbed feet trailing in the air and
  flat on the perch, a short fanned tail, a few flaps and a long glide. No two alike: size varies by a fifth, a
  brown bird's back by a quarter in tone, a white bird's bill and pouch between yellow and orange.
- **American white pelican** (the photograph's near twin, without the pink face): white, black primaries and outer
  secondaries, yellow-orange bill and pouch with a red nail, a small crest, orange legs. Flocks of 5 to 9 on
  bay-side flats under 0.4 m, on the outer part of the flat; they stand, walk and dip their bills together and
  never plunge. A few on rookery crowns.
- **Brown pelican**: adult (pale yellow crown, white neck, bill grey shading to orange-pink over a dark pouch, dark
  silver-grey back, black-brown belly, short dark legs) and, one in five, a young bird (brown head and back, pale
  belly). Perched singly on mangrove crowns, on rookeries, in lines of 3 to 5 gliding low along the edge and
  plunging on bait, and new: **hunched on an old piling standing in the water off the roots** (the second
  picture), 391 of them map-wide.
- **Mixed**: about one in four of the followers in a white flock is a brown pelican loafing with them (it keeps
  with the flock, does not dip); both kinds are on rookeries; the still pelicans on snags and dock piles are one
  in seven white and one in five young.
- Numbers map-wide (`KREco census`, `log_pk3.log`): brown pelican 2,195 (391 on pilings), white pelican 644, of
  which by the rule about 130 are the browns among the whites (not counted by a run): 3.4 browns to one white
  as counted, about 4.5 to one as drawn.

Shots (all the player's own view unless said):
| What | Shot | Note |
|---|---|---|
| a flock of white pelicans on a flat, browns among them (both kinds in view) | `pk5/KRScript_55`, `pk3/KRScript_55`, `pk3/KRScript_57` | `KREco px wflock ... white pelican (standing): 71 px tall ... 24 m from the boat; 8 of the kind in frame` (87 px at 21 m in `pk3`) |
| white pelicans taking wing and gliding low | `pk5/KREco_fly_15`, `pk5/KREco_fly_12` | about 35 m off; small in frame |
| a brown pelican perched on a piling | `pk7/KRScript_50` | `brown pelican (perched): 61 px tall ... 20 m from the boat`; it reads pale grey in that low sun, not brown |
| a brown pelican gliding | `pk7/KREco_fly_06` (past the bow, 9 m off) | pale in that light; the model's colours are plain in `pk2/KRScript_53` (TEST camera, earlier light) |
| the photograph beside the game | `shots/sheets/pelican_ref_vs_game.png` | the game half is the TEST camera on a test dummy (`kr.eco dummy`, `look dummy 5 7 0.2 0 40 -1.0`), `pk7/KRScript_52` |
| the models close (TEST camera) | `pk2/KRScript_52` white gliding, `pk2/KRScript_53` brown gliding, `pk3/KRScript_53` adult brown at rest, `pk1/KRScript_56` white at rest | before the last enlargement of bill and head; after it: `pk7/KRScript_52` to `55` |

Triangles (counted from the code): 129 a pelican on the wing near, 115 past 70 m; at rest or on the water it also
has its dark edge: 197 near, 169 far. A piling adds 12. From a run (`log_pk7.log`, off Boot Key): `KREco drawn: 36
birds (13 moving), 2582 + 3020 bird triangles in 2 meshes`. The still
pelicans on snags are part of the mangrove tile's mesh as before.

Where the likeness falls short of the photograph, seen in the side-by-side: the body is a six-sided spindle, not a
smooth long body; the primaries are five flat triangles; the head is a small faceted egg; there is no eye.

## 1. Measured before any change

The plain chase camera of the 43 ft boat sits 30.6 m behind her and 8.6 m up, with an 88 degree lens (828 pixels a
radian at 1600x900). Run `m_before` (`-KRBirdsBefore`: the birds as they were, no closer look), bow on to the
mangrove shore off Boot Key at `clock=12:00`, 25 m off (`m_before/50`) and 60 m off (`m_before/51`). Each kind in
its usual pose straight beyond the boat (`KREco px ... table` lines; the heights include 0.3 m of a wader's legs
that is drawn into the mud):

| Kind | model height | 25 m off | 60 m off |
|---|---|---|---|
| great egret | 1.97 m | 29 px | 18 px |
| great blue heron | 2.19 m | 32 | 20 |
| little blue heron | 1.37 m | 20 | 13 |
| white ibis | 1.24 m | 18 | 11 |
| roseate spoonbill | 1.55 m | 23 | 14 |
| cormorant | 1.10 m | 16 | 10 |
| anhinga | 1.38 m | 20 | 13 |
| belted kingfisher | 0.44 m | 6 | 4 |
| green heron | 0.59 m | 9 | 5 |
| night heron | 0.74 m | 11 | 7 |
| tern (across the wings) | 1.29 m | 19 | 12 |
| osprey (across the wings) | 2.55 m | 38 | 23 |
| brown pelican | 1.31 m | 19 | 12 |

What it says, and so what was done:
- **Size is not the lever.** The models at "1.5 times life" already stood taller than the birds do in life: the
  great blue heron stood 1.89 m above its feet, taller than a person on deck. It was brought DOWN (model scale 1.5
  to 1.35: 1.67 m). Only the two smallest had room to grow: kingfisher 1.9 to 2.5 (0.57 m), green heron 1.7 to 1.8.
- **The camera is the lever.** 25 m off the shore the lens was 56 m from the bird, through a wide lens. The closer
  look halves the camera's distance to the boat (15.3 m) and closes the lens to 46 degrees (1,885 pixels a radian):
  3.1 times the pixels at 25 m, with no change to the bird.
- **Contrast is the third.** Every bird at rest has a dark edge (a dark copy of itself standing just behind it) and
  a least thickness for its thin parts.

## 2. Every kind, in the player's own view

Run `pk5` (final code): `eco:goto <kind> 25` puts the boat bow on to the nearest of the kind she can lie 25 m off;
the closer look engages by itself (it is told which kind to prefer, a test hook; the camera is the player's).
`KREco px ... in frame` lines; crops of each in `shots/sheets/kinds_final.png`:

| Kind | height on screen | from the boat | shot | read in the crop |
|---|---|---|---|---|
| great egret | 293 px (the lens came to 13 m from it); 97 px in `fbirds` | 25 m | `pk5/60` | yes |
| great blue heron | 101 px | 25 m | `pk5/61` | yes: slate body, paler neck (it was a pale grey ghost before the darker colours) |
| little blue heron | 66 px | 25 m | `pk5/62` | yes: dark slate, maroon neck |
| white ibis (a party of 3) | 50 px | 33 m | `pk5/63` | yes: red bill and legs |
| roseate spoonbill | 76 px | 25 m | `pk5/64` | yes: pink |
| cormorant, drying on its snag | 53 px | 25 m | `pk5/65` | yes |
| anhinga | 68 px | 25 m | `fbirds/56` (69 px); missed in `pk5` (the shot came after the next move) | yes in `shots/sheets/kinds_25m.png` |
| belted kingfisher | 26 px perched at 29 m | | `fbirds/57`; in `pk5/67` it was on the wing | yes in `kinds_25m.png` |
| green heron | 29 px | 29 m | `pk5/68` | shape and yellow legs yes; still pale |
| night heron | 34 px | 29 m | `pk5/69` | dim, in shade |
| tern | 34 px across | 29 m | `pk5/70` | yes |
| white pelican | 71 px | 24 m | `pk5/55` | yes |
| brown pelican | 61 px perched | 20 m | `pk7/50` | shape yes, colour pale (see 9) |
| osprey | 11 px, perched | 124 m | `pk5/73` | NO: not shown near; it circles 27 m up |

A mixed shore at 60 m: `pk5/75` (`mixed60`: great egret 55 px at 59 m, anhinga on its snag 38 px at 61 m, three
brown pelicans on the wing 44 px across at 73 m); it is against the light, the dark birds read, the egret is pale.

## 3. The closer look

Chosen: the chase camera easing in by itself, not a binoculars key. Why: it needs no key, no HUD text and no
tutorial; it is one small hub change against two hub files for binoculars; and it alone gets the pixels the brief
asks for. A binoculars key with the animal's name on screen would be a good second step (it needs `KeysHUD`).

What it does (`KeysMangroveEcoView.cpp`): under about 3 kn for 1.2 s with a rookery key within 230 m of its edge or
animals of the ecosystem within 95 m, the camera goes to the far side of the boat from them and round one shoulder,
15 m from her instead of 30, 3.4 m up instead of 8.6, lens 46 to 40 degrees. A rookery is looked at from above its
crowns. With no room beside her (a creek) it stays astern and only draws in a little. It lets go above about 5 kn,
when a line is out or a fish is on, and the moment the player moves the camera, until she has been under way again.
A shower of bait within 80 m takes the eye for a few seconds.

Run `c3a` (`log_c3a.log`, 23 November):
- The pair, same place: `c3a/52` passing at speed (`making 28.5 kn`, `closer look not engaged`, lens 88) and
  `c3a/57` idle (`KREco closer look: engaged at -1150, 374, the boat making 0.0 kn: on anhinga 28 m off`; lens 46).
- Released by way on: `KREco closer look: released ... she is making N kn` (`log_m_after.log`, the tours).
- The player's hand: `cam=0.8,0.2,1.3` gives `the player moved the camera ...: it is his until she has been under
  way again`; shot `c3a/58` is his own view; 12 s later still `held off` (`c3a/59`).
- A fish on: `cast` puts the game in first person, where the closer look does not exist; no `engaged` line while
  the snook was on.

## 4. Life in and on the water (runs `fwat`, `fwat2`, 23 November; crops in `shots/sheets/water.png`)

- A school under the roots: a dark, ragged patch at the foot of the roots, 25 m and 40 m.
- A mullet's jump at 41 m: `fwat/KREco_jump_02`. A shower of bait 28 m off: `fwat/KREco_showerview_08`.
- A predator's white water: jacks at 25 m in `fwat2` (the two "jacks" crops of the sheet): white water and a
  streak; it reads as disturbed water, not plainly as a wake.
- Crabs: tree crabs on the roots from 11 m (`water.png`, "tree crabs 11m": small brown shapes on a root). NOT from
  8 m: the 43 ft boat has no room to lie 8 m off the roots with the camera beside her.

## 5. The faults left behind

- Drying cormorant and anhinga: each has a weathered post in the water under it with a bar across the top:
  `pk5/65`, `fbirds/56`.
- Fiddlers: darker, purple-brown with an orange claw: `fwat/60` (TEST camera, 2.2 m off). From the boat they are
  specks: at low tide she cannot lie nearer than 20 m.
- Kingfisher and night heron: darker upper parts, flanks the back's colour: `fbirds/57`, `pk5/69`. Ibis: a dark
  edge and redder bill and legs: `pk5/63`. Each shown from one side only.
- Rookery bookmarks: the census prints one spot per rookery, about 150 m off, in 1.6 m charted (the deepest hull in
  the boat table grounds at 1.2 m and the tide takes 0.32 m off; rookery 9 has only 1.5 m). Run `rook2`: twelve
  `KREco afloat rookeryN ... afloat` lines, none aground.

## 6. Rookeries (run `rook2`, 23 November)

From the bookmarks, 150 to 190 m off, in the player's view: `rook2/50`, `/51` (rookery 2, Lower Keys), `rook2/52`,
`/53` (rookery 7, Upper Keys), `rook2/54`, `/55` (rookery 10, Key Largo), `rook2/56`, `/57` (rookery 7 at dusk). By
day about six in ten are at home and flights of 4 to 7 leave and come in every few seconds.

## 7. Perf

Before, clean branch build of 23 November, Medium, 1600x900, editor capped at 30 fps (`log_pf_main1.log`,
`log_pf_main2.log`):

| Spot | game ms | draw ms | GPU ms |
|---|---|---|---|
| `tp=-1150,452,4.71` | 12.14, 12.14, 11.47, 13.02 | 7.56, 7.36, 6.83, 7.21 | 30.59, 30.19, 30.58, 30.58 |
| rookery 7, `tp=11030,-4734,1.102` | 9.74, 9.54, 9.71, 9.51 | 6.96, 6.77, 7.20, 7.07 | 25.79, 25.33, 23.37, 23.70 |
| Saddlebunch, `tp=-17500,-900,0` | 11.47, 11.34, 10.71, 10.77 | 7.38, 7.04, 6.84, 6.85 | 28.56, 28.18, 28.39, 28.29 |

`KREco drawn` before: flat 33-34 birds, 2,496-2,624 bird triangles, 501 instances (53,765 triangles), cpu 0.18-0.22
ms; rookery 7 96 birds, 7,104 triangles, cpu 0.06 ms; Saddlebunch 49 birds, 3,626 triangles, cpu 0.12 ms.

After: PERF_AFTER

## 8. Nothing else moves

- `KRTp checked 48 destinations, 0 bad` and `KRDocks audit: 545 docks; standing on - ... another dock 0`
  (`log_e2_on.log`, final code).
- The classic map: `log_e2_classic.log`: no `KREco` line at all, `KRTp checked 19 destinations, 0 bad`, no
  `Error:` line.
- The camera away from mangroves: at `goto=reef` the lane on gives `KREco px reef: ... lens 88.0 deg (828 px a
  radian), 30.6 m from the boat and 8.7 m above the water; closer look not engaged` (`log_e2_on.log`); with
  `-KRBirdsBefore` (no closer look at all) `30.7 m ... 8.9 m` (`log_else_before.log`, 23 November: the boat rides
  the swell, the numbers move by that much). The pair of shots, both of 23 November's build (`else_on/50`,
  `else_before/50`), side by side and the same framing: `shots/sheets/reef_on_vs_before.png`. The final code's own
  reef shot was lost (the run's first shot did not land). By the code, with
  nothing to look at `CloserLook` returns 1, 1, 0 and K 0 and `CloserFov` 88.

## Not right yet

- **Dark birds wash pale in the sun.** A sunlit rounded body goes pale tan whatever its colour (the brown pelican's
  breast, the green heron, a cormorant's back): it is the shared vertex-colour material's response to the sun, not
  the bird's colour, and it changes with the hour. Darker colours helped the flat parts (wings) and the herons;
  they do not cure a rounded body. The cure is in the material (a hub asset), not in this lane's files.
- The osprey is not shown near; a predator's wake reads as white water; crabs not from 8 m (above).
- Pelicans up off a flat glide very low for a long way before they climb.
- The test's stills after `eco:fly` come too early to catch the birds abeam.

## Done (all in the lane's own files)

- `KeysMangroveEcoView.cpp` (new): the closer look's rule and the measuring (`kr.eco px`, `afloat`, `closer`).
- `KeysMangroveEcoBirds.cpp`: the dark edge, least thickness, sizes, colours, the snag, rookery flights by day, the
  still mesh's deferral, the pelicans, the piling, pelicans kept clear of the water in flight.
- `KeysMangroveEcoWater.cpp`: a school's patch and flashes, wakes, larger splashes, larger and darker crabs.
- `KeysMangroveEco.cpp/.h`, `KeysMangroveEcoInt.h`: the white pelican kind, pilings, flocks on the outer flat,
  bookmarks with water for a boat, the rookery mix, the test commands.
- `KeysMangroveLife.cpp`: the perched pelican model's colours and mix.

## In progress

- Nothing running.

## Blocked

- Not blocked in code. Criterion 7 needs a quiet machine and a build of `main` to compare with (see 7).

## Requests for the marshal

- R1 (open): `Source/KeysRun/KeysBoatPawn.cpp` - the chase view in `UpdateCamera` asks `KeysMangroveEco::CloserLook`
  each frame and applies what it returns, and `UpdateJumpCam` takes its resting lens from
  `KeysMangroveEco::CloserFov()` instead of the constant 88. Twelve lines. With no ecosystem, away from mangroves
  or under way it returns 1, 1, 0, K 0 and 88. Checked on 24 November: the patch file is byte for byte what is
  applied in the worktree. patch: `.marshal/requests/birdview/R1.patch`
- R2 (open, one line, NOT tested, perhaps without effect): `Source/KeysRun/KeysLife.cpp` - the open-water brown
  pelicans' fallback colours to the adult's. `KeysLife` draws its pelicans from a model file
  (`Content/KeysRun/Props/bird_pelican.krfish`, `KeysLife: seabird model pelican 450 verts 900 tris` in every log)
  and uses these colours only if the file is missing, so the line may change nothing. Making the open-water
  pelicans use this lane's model is not a small request: they are drawn by `KeysLife`'s own bird code from that
  file. They stay as they are. patch: `.marshal/requests/birdview/R2.patch`
- R3 (open, a note): `KeysRun-devkit/shoot.sh` - the game lock is taken by whoever polls first; a ticket file in
  the lock directory would make it first come first served. And a game started by a script takes the keyboard:
  a key meant for another window paused one run (Esc) and put another overboard (J).
- R4 (open, compiled, NOT looked at in the game): `Source/KeysRun/KeysTownKit.cpp` - the town's pelicans (pier
  pilings, the eateries' rails, a roof) drawn by `KeysMangroveLife::Pelican`, so they are the same birds as the
  mangroves' (adults, a few young, a few white), life size. Eight lines; `-KRTownPelicanOld` gives the old ones
  back. It was applied in the worktree for the runs of 24 November but no run went to a town to look. The dock
  piles (`KeysDocks.cpp`) already use this lane's model. patch: `.marshal/requests/birdview/R4.patch`

## Guesses (the lane's own)

- "Normal play" includes the closer look: the pixel targets are met with it engaged, not in the plain chase view.
- A person on deck is 1.75 m; the tallest birds are held to 1.67 m above their feet.
- "Small ones" are the kingfisher, green heron and tern; the night heron was treated as small too (it passes).
- A tern and an osprey are measured across the wings: they are seen on the wing.
- The draught for rookery bookmarks: the deepest hull in the boat table (grounds at 1.2 m) at dead low water.
- The closer look also follows a shower of bait for a few seconds, which the brief did not ask for.
- White pelicans are in the game the year round (the game has no seasons), on the bay side only.
- A young brown pelican is one in five; a white one among the still perched browns one in seven; a brown among a
  white flock's followers one in four. The census and the pixel report call such a brown "white pelican" (it is
  one of that flock's birds).
- "On pilings, marks, snags and mangrove limbs": pilings (new, the lane's own posts), snags and dock piles (the
  still model) and crowns are done; channel marks are not (they are hub scenery).
- The herons and the brown pelican were set darker than their true colours to stand the game's sun.

## Notes for integration

- Apply R1 and R4 with `git apply`. Test commands (console `kr.eco ...` or script `eco:...`): `px [tag]`,
  `afloat [tag]`, `closer [on|off|want <kind>]`, `goto <kind> [dist]` (now falls back to the nearest she can lie
  to; `goto piling` finds a pelican's piling), `fly <kind>`, `jump`, `shower`, `dummy` / `look dummy <n> <dist>
  <height> <side> <fov> [quarter]` (six pelican dummies beside the boat), `census all` (now also prints three
  pilings and three white flocks a region with a bookmark each), and the old ones.
- Bookmarks used: a white flock `tp=-5017,-4546,2.095` (Middle Keys bay side); a pelican's piling
  `tp=-8224,-2090,-1.253`.
- `-KRBirdsBefore` draws the birds as they were before this lane and switches the closer look off (for measuring);
  it does not take away the white pelicans, the pilings or the rookery mix.
- `EKind` gained `WhitePelican` after `Pelican`: every later kind's number moved up by one. Nothing outside the
  lane's files uses the numbers (checked by grep).
- The game's sun follows the real clock; `clock=12:00` sets the hour the animals keep, not the light. Shots taken
  at different real hours differ in light.
- The boat table in `KeysData.cpp` and the HUD show real boat and engine brand names (seen, not this lane's).
- Scratch scripts used for the runs are in the session's scratchpad, not in the repo.
