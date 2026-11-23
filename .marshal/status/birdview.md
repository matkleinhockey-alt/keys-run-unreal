# birdview

Status: IN PROGRESS
Updated: 2026-11-23 10:10 (interim: evidence runs for criteria 3 to 8 are queued behind another session's game runs)
Branch: lane/birdview (see `git log -1 lane/birdview`)

The user picked this from a list, in the marshal's words: "Bring the camera in or enlarge the birds so the new
mangrove wildlife is actually visible from the boat." The goal, the deliverables and every acceptance criterion
below are the marshal's reading of that, not the user's words. What this lane had to guess is under "Guesses".

Evidence is outside the repo in `/Users/mat/Documents/Unreal Projects/KeysRun-devkit/lanes/KeysRun-birdview/`:
`shots/<tag>/KRScript_NN.png` and `log_<tag>.log`. All runs had request R1 (the hub patch) applied locally; it is
not committed.

## Acceptance criteria

- [ ] 1. Measure first - measured (table below); to be re-run on the final build before it is ticked.
- [ ] 2. Every kind can be seen in normal play - working (tags `tourM`, `tourB`); final shots owed.
- [ ] 3. A closer look, for the player - working; the pass / idle pair, the override run and the fish-on run are queued.
- [ ] 4. Life in and on the water reads too - written, not yet looked at.
- [ ] 5. The faults left behind - snag, colours and bookmarks written; shots and the twelve-bookmark run queued.
- [ ] 6. Rookeries are a sight - written, not yet looked at.
- [ ] 7. No slowdown - the "before" samples are taken (below); "after" owed.
- [ ] 8. Nothing else moves - not run yet.

## Measured before any change (criterion 1)

The save's boat is the 43 ft one: its plain chase camera sits 30.6 m behind the boat and 8.6 m up, with an 88 degree
lens (828 pixels a radian at 1600x900). Run `m_before` (`-KRBirdsBefore`: the birds as they were, no closer look),
bow on to the mangrove shore off Boot Key at `clock=12:00`, shots `m_before/50` (25 m off) and `m_before/51` (60 m).
Each kind in its usual pose, straight beyond the boat (`KREco px ... table` in `log_m_before.log`; the heights
include the 0.3 m of a wader's legs that is drawn into the mud):

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

Birds actually in that frame: a cormorant 10 px tall at 29 m, a night heron 7 px at 64 m, a tern 7 px across at 45 m.

What that says:
- **Size is not the lever.** The models at "1.5 times life" already stand taller than the birds do (the necks are
  drawn upright): the great blue heron stood 1.89 m above its feet, taller than a person on deck. Making the tall
  ones larger would break the brief's own limit. Only the two smallest (kingfisher, green heron) had room to grow.
- **The camera is the lever.** 25 m off the shore the lens is 56 m from the bird, through a wide lens. Halving the
  camera's distance to the boat and closing the lens from 88 to about 46 degrees gives 3.1 times the pixels: a great
  egret goes from under 30 px to 95, without touching the bird.
- **Contrast is the third.** White birds stand on water that is near white from a low angle, and legs, necks and
  bills are under a pixel wide at 40 m. A dark edge and a least thickness for thin parts fix what size cannot.

## Perf, before (criterion 7)

Clean branch build, Medium, 1600x900, editor capped at 30 fps (`log_pf_main1.log`, `log_pf_main2.log`; two samples
each run):

| Spot | game ms | draw ms | GPU ms |
|---|---|---|---|
| `tp=-1150,452,4.71` | 12.14, 12.14, 11.47, 13.02 | 7.56, 7.36, 6.83, 7.21 | 30.59, 30.19, 30.58, 30.58 |
| rookery 7, `tp=11030,-4734,1.102` | 9.74, 9.54, 9.71, 9.51 | 6.96, 6.77, 7.20, 7.07 | 25.79, 25.33, 23.37, 23.70 |
| Saddlebunch, `tp=-17500,-900,0` | 11.47, 11.34, 10.71, 10.77 | 7.38, 7.04, 6.84, 6.85 | 28.56, 28.18, 28.39, 28.29 |

`KREco drawn` before: flat 33-34 birds, 2,496-2,624 bird triangles, 501 instances (53,765 triangles); rookery 7
96 birds, 7,104 triangles; Saddlebunch 49 birds, 3,626 triangles.

## Done (so far; all in the lane's own files)

- `KeysMangroveEcoView.cpp` (new): the closer look's rule (`CloserLook`, `CloserFov`: what to look at, where the lens
  stands clear of the hull, trees and docks, when it engages and lets go) and the measuring (`kr.eco px`,
  `afloat`, `closer`).
- `KeysMangroveEcoBirds.cpp`: a dark edge round every bird, a least thickness for legs, necks and bills, the great
  blue heron lowered, the kingfisher and green heron larger, darker kingfisher / night heron / great blue / spoonbill
  colours, a snag under a drying cormorant or anhinga, rookery flights both ways by day.
- `KeysMangroveEcoWater.cpp`: a school's dark patch and silver flashes at the surface, a wake behind a fish under
  way, larger splashes for a mullet's jump, a shower and a rush, larger and darker crabs.
- `KeysMangroveEco.cpp`: bookmarks with water for a boat (`find`, `goto`, the rookery spots in the census), the
  rookery mix (more white and pink), test commands `goto`, `jump`, stills of an event from the player's view.

## In progress

- The evidence runs for criteria 2 to 8 on the final build.

## Blocked

- Nothing. (Another session is running long scripted drives on this machine; each of this lane's runs waits its turn.)

## Requests for the marshal

- R1 (open): `Source/KeysRun/KeysBoatPawn.cpp` - the chase view in `UpdateCamera` asks `KeysMangroveEco::CloserLook`
  each frame and applies what it returns (distance and height multipliers, an angle, a look point blended by K), and
  `UpdateJumpCam` takes its resting lens from `KeysMangroveEco::CloserFov()` instead of the constant 88. Twelve
  lines. With no ecosystem, away from mangroves or under way it returns 1, 1, 0, K 0 and 88: the view is unchanged.
  patch: `.marshal/requests/birdview/R1.patch`

## Guesses

- (to be completed)

## Notes for integration

- (to be completed)
