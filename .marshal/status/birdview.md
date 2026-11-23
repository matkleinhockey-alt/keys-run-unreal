# birdview

Status: IN PROGRESS
Updated: 2026-11-23 11:15
Branch: lane/birdview (see `git log -1 lane/birdview`)

The user picked this from a list, in the marshal's words: "Bring the camera in or enlarge the birds so the new
mangrove wildlife is actually visible from the boat." The goal, the deliverables and criteria 1 to 8 below are the
marshal's reading of that, not the user's words. Criterion 9 (pelicans) came during the work: the user's words were
"use this as a model for pelicans" (with a photograph, `KeysRun-devkit/refs/pelican_ref.png`), "and this one for
others, mix them up" (a second photograph that reached this lane only as the marshal's description), and the
standing rule "i just want to keep it realistic". How those were read is the marshal's reading, set out under
criterion 9. What this lane itself had to guess is under "Guesses".

Evidence is outside the repo in `/Users/mat/Documents/Unreal Projects/KeysRun-devkit/lanes/KeysRun-birdview/`:
`shots/<tag>/KRScript_NN.png` (and `KREco_*.png`) and `log_<tag>.log`. Every run had request R1 (the hub patch)
applied locally; it is not committed. The save's boat is the 43 ft one. No `verifier` agent type was available in
this session: the lane followed the verifier's brief itself (every shot cited was opened; what was not checked is
said).

**State of the evidence, read this first.** Another session ran long scripted drives on this machine through most
of this lane's time and the game lock is not first come first served: single runs of this lane waited 15 to 35
minutes. The code for every criterion is written and builds. The runs that back criteria 1 to 6 below were made on
builds before the last changes (the pelican rebuild, the still-mesh deferral for criterion 7); the final-build runs
for criteria 2, 4, 7, 8 and 9 had not come back when this was written. Ticks below are only for what a run on some
build of this lane showed; "owed" says what is still to run.

## Acceptance criteria

- [x] 1. Measure first - table and reasoning below (`log_m_before.log`, shots `m_before/50`, `m_before/51`).
- [ ] 2. Every kind can be seen in normal play - shown for 11 of 13 kinds at 25 to 33 m (`tourM`, `tourB`: table
  below); tern and osprey were only caught far off; the mixed shore at 60 m and the final-build sheet are owed.
- [x] 3. A closer look, for the player - `c3a` (below). One wording gap, since fixed in code and not yet re-run:
  when a line went out the look ended without a "released" line (the game goes to first person).
- [ ] 4. Life in and on the water reads too - a school's dark patch, a mullet's jump and a shower are shown
  (`fwat`); a predator's wake and crabs from 8 m are NOT shown yet.
- [ ] 5. The faults left behind - snag, bookmarks and fiddlers shown; ibis, kingfisher and night heron shown from
  one side only.
- [x] 6. Rookeries are a sight - `rook2` (below).
- [ ] 7. No slowdown - "before" measured; "after" owed. The ecosystem's own cost at the rookeries is measured.
- [ ] 8. Nothing else moves - owed (the script is written: `tpcheck`, `dk:audit`, the reef pair, the classic map).
- [ ] 9. Pelicans on the user's pictures - written and built, not yet seen in a single shot.

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

Birds really in that frame: a cormorant 10 px tall at 29 m, a night heron 7 px at 64 m, a tern 7 px across at 45 m.

What it says, and so what was done:
- **Size is not the lever.** The models at "1.5 times life" already stood taller than the birds do in life (their
  necks are drawn upright): the great blue heron stood 1.89 m above its feet, taller than a person on deck, which
  the brief forbids. It was brought DOWN (model scale 1.5 to 1.35: 1.67 m, the same as the great egret). Only the
  two smallest had room to grow: kingfisher 1.9 to 2.5 (0.57 m), green heron 1.7 to 1.8 (0.62 m). The rest are
  unchanged.
- **The camera is the lever.** 25 m off the shore the lens was 56 m from the bird, through a wide lens. The closer
  look halves the camera's distance to the boat (15.3 m) and closes the lens to 46 degrees (1,885 pixels a radian):
  3.1 times the pixels at 25 m, with no change to the bird.
- **Contrast is the third.** From a low camera the water is near white, and legs, necks and bills are under a pixel
  at 40 m. Every bird now has a dark edge (a dark copy of itself standing just behind it: the vertex colour
  material is two-sided, so a hull turned inside out hid the bird) and a least thickness for its thin parts.

## 2. Every kind, in the player's own view

Runs `tourM` (Middle Keys bay side) and `tourB` (off Boot Key): `eco:goto <kind> 25` puts the boat bow on to the
nearest of the kind she can lie 25 m off in 1.1 m of water; the closer look then engages by itself (it is told
which kind to prefer, a test hook; the camera is the player's). `KREco px ... in frame` lines:

| Kind | height on screen | from the boat | shot |
|---|---|---|---|
| great egret | 95 px | 25 m | `tourM/50` |
| great blue heron | 102 px | 25 m | `tourM/51` |
| little blue heron | 66 px | 25 m | `tourM/52` |
| white ibis (a party of 3) | 50 px | 33 m | `tourM/53` |
| roseate spoonbill | 76 px | 25 m | `tourM/54` |
| cormorant, swimming | 27 px (low in the water) | 36 m | `tourM/55`; on its snag `tour1/56` |
| anhinga | 67 px | 25 m | `tourM/56` |
| belted kingfisher | 26 px at 29 m, 28 px at 25 m | | `tourM/57`, `tourB/57` |
| green heron | 29 px at 29 m, 24 px at 25 m | | `tourM/58`, `tourB/58` |
| night heron | 34 px at 29 m, 38 px at 25 m | | `tourM/59`, `tourB/59` |
| brown pelican (the old model) | 68 px | 25 m | `tourM/62` (hidden by the boat in that shot: fixed after) |
| tern | 21 px across at 67 m; 64 px across at 24 m in `tour3` | | NOT caught near and centred |
| osprey | 12 px, perched 102 m off | | NOT shown well |

All large waders are over 40 px and the three small perching kinds are at or over 24 px at 25 m. Looked at, in
crops: the great egret, little blue, anhinga, night heron and the ibis party read as those birds; the spoonbill
and great blue heron washed out pale in full sun (both darkened afterwards, not yet re-shot); the green heron
reads pale. Shots at 60 m: `m_after/51` (an earlier build; a mixed shore with the dark birds readable, the white
ones small). The final-build contact sheet and the 60 m mixed shore are owed.

## 3. The closer look

Chosen: the chase camera easing in by itself, not a binoculars key. Why: it needs no key, no HUD text and no
tutorial; it is one small hub change against two hub files for binoculars; and it alone gets the pixels the brief
asks for. A binoculars key with the animal's name on screen would be a good second step (it needs `KeysHUD`).

What it does (`KeysMangroveEcoView.cpp`): under about 3 kn for 1.2 s with a rookery key within 230 m of its edge or
animals of the ecosystem within 95 m, the camera goes to the far side of the boat from them and round one shoulder
(so her middle is just out of frame and the line of sight clears her whole hull, trees, ground and docks), 15 m from
her instead of 30, 3.4 m up instead of 8.6, lens 46 to 40 degrees. A rookery is looked at from above its crowns
(17 to 19 m up). With no room beside her (a creek) it stays astern and above and only draws in a little. It lets
go above about 5 kn, when a line is out or a fish is on, and the moment the player moves the camera, until she has
been under way again. A shower of bait within 80 m takes the eye for a few seconds.

Run `c3a` (`log_c3a.log`):
- The pair, same place: `c3a/52` passing at speed (`KREco afloat pass30: the boat at -1164, 374 ... making 28.5 kn`,
  `closer look not engaged`, lens 88, 34 m from the boat) and `c3a/57` idle at -1150, 374 (`KREco closer look:
  engaged at -1150, 374, the boat making 0.0 kn: on anhinga 28 m off`; lens 46, 15.3 m from the boat, 3.5 m up).
- Released by way on: in `m_after` and the tours, `KREco closer look: released ... she is making N kn` (see also
  `log_rook2.log` for `released ... nothing to look at here any more` after a teleport).
- The player's hand: `cam=0.8,0.2,1.3` gives `KREco closer look: the player moved the camera (yaw 0.80, pitch 0.20,
  zoom 1.30): it is his until she has been under way again` and `released ... the player moved the camera`; shot
  `c3a/58` is his own view (lens 88, 39.8 m, 13 m up); 12 s later still `held off` (`c3a/59`). After a run under
  power it engages again (`engaged 2 times`).
- A fish on: `cast` puts the game in first person, where the closer look does not exist; the snook was on from
  15.22.43 to 15.22.46 with no `engaged` line; it engaged again a second after the fish was lost.
- A fault found and fixed here: slowing through 5 m/s was taken for the player's hand (the pawn swings the camera
  astern above that speed). An earlier `c3a` run shows it; the run cited does not.

## 4. Life in and on the water (run `fwat`, an earlier build)

- A school under the roots: a dark, ragged patch at the foot of the roots, `fwat/50` (25 m) and `fwat/51` (40 m).
  The silver flashes on it are faint in the stills (they were enlarged afterwards; not re-shot).
- A mullet's jump at 41 m: `fwat/KREco_jump_02` (a column of spray over a metre high).
- A shower of bait 28 m off: `fwat/KREco_showerview_08`, `_10` (white water thrown up).
- A predator's wake: drawn (a V of foam behind any fish under way near the top), NOT shown: no shot caught one.
- Crabs from 8 m: NOT shown. Lying 8 m off the roots the camera has no room beside a 43 ft boat and stays astern
  (`fwat/58`: the roots are there, the crabs cannot be made out). Tree crabs are now 14 to 18 cm (were 8 to 9).

## 5. The faults left behind

- Drying cormorant and anhinga: each now has a weathered post standing in the water under it with a bar across the
  top, along the shore: `tour1/56`, `tourM/56`.
- Fiddlers: darker, purple-brown with an orange claw: `fwat/60` (the lane's test camera, 2.2 m off). From the boat
  they are specks: at low tide she cannot lie nearer than 20 m.
- Kingfisher and night heron: darker upper parts, and a bird's flanks are now its back's colour, not a wash of back
  and belly: `tourM/57`, `tourM/59`, `tour3/59`. Ibis: a dark edge and redder bill and legs: `tourM/53`. Each shown
  from one side only.
- Rookery bookmarks: the census now prints one spot per rookery, about 150 m off, where the chart shows 1.6 m with
  the same all round (the deepest hull in the boat table grounds at 1.2 m and the tide takes 0.32 m off the chart;
  rookery 9 has only 1.5 m). Run `rook2`: twelve `KREco afloat rookeryN ... afloat` lines, none aground (the save's
  boat grounds at 0.52 m; least water under her 1.35 m).

## 6. Rookeries (run `rook2`)

From the bookmarks, 150 to 190 m off, in the player's view: `rook2/50`, `/51` (rookery 2, Lower Keys), `rook2/52`,
`/53` (rookery 7, Upper Keys: dozens of white birds on the crowns), `rook2/54`, `/55` (rookery 10, Key Largo),
`rook2/56`, `/57` (rookery 7 at dusk). By day about six in ten are at home and each bird's turn out on the flats
comes round every seven minutes: `KREco rookery 7 (Upper Keys) ... day: a flight of 5 leaves for the flats` /
`a flight of 4 comes in to roost` every few seconds, 10 on the wing at once. The mix is whiter and pinker
(egret 28%, ibis 24%, spoonbill 16%).

## 7. Perf

Before, clean branch build, Medium, 1600x900, editor capped at 30 fps (`log_pf_main1.log`, `log_pf_main2.log`):

| Spot | game ms | draw ms | GPU ms |
|---|---|---|---|
| `tp=-1150,452,4.71` | 12.14, 12.14, 11.47, 13.02 | 7.56, 7.36, 6.83, 7.21 | 30.59, 30.19, 30.58, 30.58 |
| rookery 7, `tp=11030,-4734,1.102` | 9.74, 9.54, 9.71, 9.51 | 6.96, 6.77, 7.20, 7.07 | 25.79, 25.33, 23.37, 23.70 |
| Saddlebunch, `tp=-17500,-900,0` | 11.47, 11.34, 10.71, 10.77 | 7.38, 7.04, 6.84, 6.85 | 28.56, 28.18, 28.39, 28.29 |

`KREco drawn` before: flat 33-34 birds, 2,496-2,624 bird triangles, 501 instances (53,765 triangles), cpu 0.18-0.22
ms; rookery 7 96 birds, 7,104 triangles, cpu 0.06 ms; Saddlebunch 49 birds, 3,626 triangles, cpu 0.12 ms.

After: OWED (script `perf.sh`, the same samples). What is known: the ecosystem's own cost at the rookery bookmarks
was 0.88 to 1.00 ms a frame with day flights (the still mesh was laid out again at every take-off and landing);
with far birds deferred to once a second it is 0.17 to 0.21 ms (`log_rook2.log`: rookery 7 139 birds, 16,526
triangles; rookery 10 173 birds, 20,820 triangles). Bird triangles are about double (the dark edge).

## 9. Pelicans (added during the work)

The marshal's reading, stated as a reading: the bird in the user's photograph is a great white pelican (Africa); its
near twin in the Keys is the American white pelican, a winter bird of Florida Bay, so that is what was added, built
on the photograph but without the pink face the American bird lacks. The brown pelican stays the common one and is
rebuilt on the second picture (adult: pale yellow crown, white neck, bill grey at the base and orange-pink toward
the tip over a dark grey pouch, silver grey-brown back, dark belly, short dark legs).

Written and built (`EmitPelican` in `KeysMangroveEcoBirds.cpp`, `KeysMangroveLife::Pelican`), NOT YET SEEN:
- one model for both: pouched bill with a hooked nail, the neck tucked on the shoulders in flight, two-panel wings
  with coverts along the leading edge, flight feathers behind and five primaries spread at the tip, a folded wing
  at rest whose dark tip hangs past the tail, webbed feet trailing in the air and flat on the perch, a fanned tail,
  a few flaps and a long glide;
- white pelicans: flocks of 5 to 9 on bay-side flats under 0.4 m, standing, walking and dipping their bills
  together (they never plunge); a few on rookeries;
- brown pelicans: perched singly as before, on rookeries, and now in lines of 3 to 5 gliding low along the edge,
  plunging on bait; one in five is a young bird (brown head, pale belly); sizes vary by a fifth;
- the perched pelicans built into the mangrove tiles (snags, crowns) recoloured to the adult, with young and white
  ones among them.
Owed: every shot the criterion asks for, the side-by-side with the photograph, the triangle count from a run
(by the code: about 130 triangles a pelican, 195 with its dark edge), the map-wide counts of the two kinds.

## Done (all in the lane's own files)

- `KeysMangroveEcoView.cpp` (new): the closer look's rule and the measuring (`kr.eco px`, `afloat`, `closer`).
- `KeysMangroveEcoBirds.cpp`: the dark edge, least thickness, sizes, colours, the snag, rookery flights by day, the
  still mesh's deferral, the pelicans.
- `KeysMangroveEcoWater.cpp`: a school's patch and flashes, wakes, larger splashes, larger and darker crabs.
- `KeysMangroveEco.cpp/.h`, `KeysMangroveEcoInt.h`: the white pelican kind, bookmarks with water for a boat, the
  rookery mix, test commands `goto`, `fly`, `jump`, `shower` stills, `px`, `afloat`, `closer`.
- `KeysMangroveLife.cpp`: the perched pelican model.

## In progress

- The final-build evidence runs (scripts are written; they wait on the game lock).

## Blocked

- Not blocked in code. Slowed badly by the game lock (see the top).

## Requests for the marshal

- R1 (open): `Source/KeysRun/KeysBoatPawn.cpp` - the chase view in `UpdateCamera` asks `KeysMangroveEco::CloserLook`
  each frame and applies what it returns, and `UpdateJumpCam` takes its resting lens from
  `KeysMangroveEco::CloserFov()` instead of the constant 88. Twelve lines. With no ecosystem, away from mangroves
  or under way it returns 1, 1, 0, K 0 and 88. patch: `.marshal/requests/birdview/R1.patch`
- R2 (open, one line, NOT tested): `Source/KeysRun/KeysLife.cpp` - the open-water brown pelicans' colours to the
  adult's (dark body, silver-grey wings, pale yellow head) so they match the mangrove ones. Their shape stays as it
  is; white pelicans are bay birds and do not belong offshore. patch: `.marshal/requests/birdview/R2.patch`
- R3 (open, a note): `KeysRun-devkit/shoot.sh` - the game lock is taken by whoever polls first; a lane's two-minute
  run can wait behind three fifteen-minute drives. A ticket file in the lock directory would make it first come
  first served.

## Guesses (the lane's own)

- "Normal play" includes the closer look: the pixel targets are met with it engaged, not in the plain chase view.
- A person on deck is 1.75 m; the tallest birds are held to 1.67 m above their feet.
- "Small ones" are the kingfisher, green heron and tern; the night heron was treated as small too (it passes).
- A tern and an osprey are measured across the wings: they are seen on the wing.
- The draught for rookery bookmarks: the deepest hull in the boat table (grounds at 1.2 m) at dead low water.
- The closer look also follows a shower of bait for a few seconds, which the brief did not ask for.
- White pelicans are in the game the year round (the game has no seasons), on the bay side only.
- A young brown pelican is one in five; a white one among perched browns one in seven.

## Notes for integration

- Apply R1 with `git apply`. Test commands (console `kr.eco ...` or script `eco:...`): `px [tag]`, `afloat [tag]`,
  `closer [on|off|want <kind>]`, `goto <kind> [dist]`, `fly <kind>`, `jump`, `shower`, and the old ones.
- `-KRBirdsBefore` draws the birds as they were before this lane and switches the closer look off (for measuring).
- `EKind` gained `WhitePelican` after `Pelican`: every later kind's number moved up by one. Nothing outside the
  lane's files uses the numbers (checked by grep: the hub uses names and functions only).
- The boat table in `KeysData.cpp` and the HUD show real boat and engine brand names (seen, not this lane's).
- Scratch scripts used for the runs are in the session's scratchpad, not in the repo.
