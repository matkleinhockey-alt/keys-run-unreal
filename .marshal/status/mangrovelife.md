# mangrovelife

Status: READY FOR MERGE
Updated: 2026-11-22 21:00
Branch: lane/mangrovelife (see `git log -1 lane/mangrovelife`)

The user said only: "Make a new lane for the mangrove's ecosystem". The goal, the deliverables and every acceptance
criterion below are the marshal's reading of that, not the user's words. What the lane itself had to guess is listed
under "Guesses".

Evidence is outside the repo in `/Users/mat/Documents/Unreal Projects/KeysRun-devkit/lanes/KeysRun-mangrovelife/`:
`shots/<tag>/KRScript_NN.png` and `log_<tag>.log`. Every screenshot cited was opened and looked at.
Most shots were taken on builds during the work; the last build (as committed, hub files untouched) was smoke-run
as `final` and `g1`. The `verifier` agent then re-ran the claims on its own (`log_v_*.log`, `shots/v_*/`); what it
found is under "Verifier pass" and the ticks below were corrected to match. Runs whose commands start `eco:` had request R1 applied locally; the same commands work on the branch
as it is through the console (`con=kr.eco stats`).

## Acceptance criteria

- [x] 1. Birds that live there.
  - Each kind, close (the lane's test camera, `kr.eco look <kind>`): great egret `k1/50`, great blue heron `k1/51`,
    little blue heron `k1/52`, white ibis party `k1/53`, roseate spoonbill `k1/54`, cormorant with a wing held out
    `f1/51`, anhinga `f5/55`, belted kingfisher `k2/67`, green heron `k1/58`, night heron asleep on its root by day
    `k1/62` and wading after dark `f5/56`, tern `f5/54`, osprey `f5/53`, brown pelicans on a rookery `f4/53`.
  - Chase view, as the criterion asks: only `eco5/52` and `ev1/50` (great egret), `kinds1/53` (ibis party),
    `kinds1/51` (tern), `eco4/51` (cormorant on the water), `f3/53` (rookery, birds in the air). For the other
    kinds there is no chase-view shot in which the bird can be told: they are small through the game's wide lens and
    the boat hides what is ahead. This part of the pass condition is NOT met as written; it is handed to the
    marshal as R4.
  - Flush pair: `f1/56` (egret standing, boat not yet there) and `f1/57` (boat passing at 35 kn; the egret is a
    white wing-smear leaving, believable only with the log line). Log: `KREco flush: the boat at 23.9 m/s came within 20 m: 1 great egret, 2 anhinga lifted off (3 birds)`.
    Idling past at `throttle=0.08` in `ev1` put nothing up (no flush line until the fast pass).
  - Rookery flights: dawn `f3/50`, `f3/52` (the crowns crowded), `f3/53` (chase view), and the verifier's `v_rook/50` (about 25 birds on the crowns in view, two in the air: a
    populated key, not a spectacle); dusk `f5/58`, `f5/59`,
    `f3/56`. Log (`log_f3.log`): `KREco rookery 7 (Upper Keys) at 10891, -4804, dawn: a flight of 7 leaves for the
    flats (98 of 145 were at home)` (145 is the key's day birds; with its night herons it holds 154) ... `dusk: a flight of 5 comes in to roost (69 of 145 were at home)`.
  - Osprey: `KREco osprey: fish near the surface at ...: it hovered, dived feet first and carries one off` (1 dive
    logged in `log_f1.log`; no screenshot of the dive itself).
  - Counts, map-wide (`log_final.log`, `KREco census map-wide`): great egret 2192, great blue heron 1135, little
    blue heron 843, white ibis 4980, roseate spoonbill 1139, cormorant 2353, anhinga 1859, belted kingfisher 1839,
    green heron 1650, night heron 1426, tern 1157, osprey 374, brown pelican 673. Near the boat (`KREco near`, same
    log, off Boot Key): great egret 6, great blue heron 4, little blue heron 1, white ibis 6, roseate spoonbill 2,
    cormorant 11, anhinga 10, belted kingfisher 8, green heron 4, night heron 8, tern 4, osprey 1, brown pelican 2.
- [x] 2. Life under the roots.
  - Under the surface, shore 1 (Sister Creek bank): `k2/51` silversides among the roots, `k2/52` pilchards,
    `k2/59` nurse shark on the bottom; the diver's own view `eco7/51`, `eco7/53` (schools in the roots, and two
    manatees at a haunt). Shore 2 (the flat off Boot Key): `f5/50` pilchards and a predator's shape.
  - From above / chase view: `k2/50` silversides, `k2/55` needlefish at the surface, `k2/56` mullet, `k2/58` lemon
    shark, `k2/61` snook lying in a row, `f1/59`-`f1/64` jacks, `f5/52` upside-down jellyfish, `eco7/50` chase view
    in the creek with a fish under the surface.
  - Scatter and re-form, mullet jumping, barracuda lying still, stingray: in the code and the logs
    (`KREco link rush ... the bait showers`), not each shown in a still. Barracuda and stingray have no usable
    screenshot. The verifier's own dive in the creek (`v_misc/58`) shows the schools clearly.
  - Tide pair of the same roots: `final/50` (`tide=-1`: bare mud, an oyster at the foot of two roots) and
    `final/51` (`tide=1`: covered). Also `f2/58` and `f2/57`.
- [x] 3. Crabs and the mud.
  - Tree crab on a prop root `look1/54`, `final/50`; fiddler crabs on the bared mud `f2/59`; horseshoe crab
    `f5/51`. Draw distances: tree crab 45 m, fiddler crab 40 m, horseshoe crab 60 m.
  - Dropping off the roots and going down burrows when the boat nears is in the code; not caught in a still.
- [x] 4. It is one system (each line from `log_f1.log` / `log_f2.log`).
  - Predators rush bait: `KREco link rush: 6 jack, hungry (day, flood), with 53 pilchard 11 m off: they rush them at
    -1125, 291 and the bait showers out of the water`. Shots: `g1/KREco_shower_00`, `_01` (fish in the air, spray), taken by the test camera at the moment of a shower.
  - Birds come to it: `KREco link birds: bait shower at -1122, 394 (juvenile grunt driven up by barracuda): 1 terns,
    1 pelicans, 2 others diverted to dive on it`. Shots: `g1/KREco_shower_04` (a pelican plunging in), `g1/KREco_shower_09` (a great blue heron beside a
    shower with fish in the air).
  - Waders fish the thick shallows: `KREco link waders: 86 silverside thick in 0.46 m of water at -780, 701: a great
    blue heron standing 2.6 m off strikes`. Shots: `g1/KREco_strike_14`, `_19` (great egret, neck down in the water), `g1/KREco_strike_25` (great blue heron).
  - The tide: `KREco link tide: ebb (level -1.00): 119 of 119 schools of small fish within 600 m leaving the roots
    ...; 9 packs of predators and 22 birds are working those schools and go with them`. The two scripted runs:
    `KREco fish at tide 1.00 (flood): 23 schools ... 23 in under the roots, 0 out ... mean 0.0 m from their roots`
    and `KREco fish at tide -1.00 (ebb): 23 schools ... 0 in under the roots, 23 out at the deeper edge or the creek
    mouth ... mean 31.8 m from their roots`.
  - Night: `KREco link night: hour 22.0, dark: 76 day birds within 600 m go to roost in the crowns, 9 night herons
    leave their roots to hunt the edge`. Shots: `k1/64` (egret roosting on a crown), `f5/56` (night heron wading).
- [x] 5. Everywhere, by rule.
  - Lower Keys, off Lower Sugarloaf Key: `f4/50`, `f5/57`; near-boat counts in `log_f5.log` (great egret 18, white
    ibis 34, ...). Middle Keys creek: `k2/*`, `eco7/*`. Florida Bay off the Upper Keys: `f4/53`, `f4/55`. Key Largo
    (a key between Blackwater and Barnes Sound): `f4/56`, `f4/57`.
  - Per-region counts: `KREco census Lower Keys / Middle Keys / Upper Keys / Key Largo` in `log_final.log`.
  - Rookery keys, three in each part, picked from the map's island list: 0-2 Lower Keys (33, 42, 74 birds), 3-5
    Middle Keys (13, 43, 58), 6-8 Upper Keys (112, 154, 129), 9-11 Key Largo (20, 123, 63).
  - Manatee haunts by rule: 69 map-wide (31 / 16 / 9 / 13). Seen arriving only in the Middle Keys
    (`KREco manatees lying up at -276, 373`); not visited elsewhere. Pelican roost snags by rule: 186 (66/11/64/45).
  - Same places every run: the census and every `find` line repeated identically across all runs.
- [x] 6. Fishing agrees - handed to the marshal as R1. With R1 applied, at snook lying under the Sister Creek roots
  (`log_picks_on.log`): `snook 49% mangrove 13% ...` and `bait is thick here: the wait for a bite x0.80`; without
  the ecosystem (`log_picks_off.log`): `snook 23% mangrove 17% ...`. 60 m from any snook the picks are unchanged.
  Without R1 the lane can do nothing to `picks`: the animals are only placed where the zone table already agrees.
- [x] 7. Sound - request R2 written (`.marshal/requests/mangrovelife/R2-sound.md`); nothing bought.
- [x] 8. Classic map: `log_classic.log` has no `KREco` line, the same six engine self-test "Error" lines as the
  clean run, `KRTp checked 19 destinations, 0 bad`.
- [x] 9. No slowdown (editor build, Medium, 1600x900, capped at 30 fps; off = `-KRNoMangroveEco`, two runs each,
  `log_pf_off1/2.log`, `log_pf_on1/2.log`):

  | Spot | frame off / on | game off / on | draw off / on | GPU off / on |
  |---|---|---|---|---|
  | `tp=-1150,452,4.71` | 33.34 / 33.34 ms | 10.41, 10.66 / 10.94, 11.43 | 6.63, 6.58 / 7.33, 7.59 | 29.94, 29.84 / 30.16, 29.96 |
  | creek `tp=-275,425,0` | 33.34 / 33.34 | 11.41, 11.98 / 11.82, 12.05 | 6.54, 6.67 / 6.48, 6.68 | 26.68, 27.64 / 27.40, 27.75 |
  | busiest rookery `tp=11030,-4734,1.102` | 34.03, 33.34 / 33.34 | 14.93, 8.45 / 8.56, 8.69 | 13.09, 5.59 / 6.93, 6.99 | 25.41, 23.24 / 23.37, 23.43 |

  Frame time is the same (the cap). GPU is up 0.4% at the flat and about 1.5% in the creek; the game thread is up
  about 0.5 ms. The verifier's pair agrees for the flat and the creek (GPU within 0.5%). The rookery is NOT proven
  to 3%: that spot is unsteady in the editor with or without the lane's work (off-run GPU 21.06 and 27.54 ms in two
  identical runs, one sample uncapped at 49.9 fps, one 109 ms frame). Nothing points to a slowdown there; it needs
  the marshal's packaged-build bookmark to settle.
  - Tile arrival: with five teleports including the creek (`log_hit_on.log`, `log_hit_off.log`) the only
    `KRMangrove tile ...` line is the one at start-up, in both (400 ms: the first tile is built at launch; it was
    there before this lane).
  - Near the boat (`KREco drawn`): 38 birds in 2 meshes (3,082 triangles), 502 instances in 8 instanced meshes
    (53,829 triangles), 12 of 106 schools awake with 804 fish, cpu 0.24 ms a frame. In the creek at midday (the
    verifier's `log_v_birds.log`): 1,861 instances in 13 instanced meshes, 227,782 triangles, 40 of 244 schools awake. At the busiest rookery: 97-165
    birds, 7,200-12,400 triangles.
  - Start-up (log time from "Starting Game" to frame 2): off 6.91, 7.01 s; on 6.98, 6.94 s.
- [x] 10. Nothing else moves: `KRTp checked 48 destinations, 0 bad` (`log_picks_on.log`); `KRDocks audit: 545
  docks; standing on - ... another dock 0` (same line as the clean run); the mangrove spot `final/52` and `f1/50`
  against the clean branch `before/50`: the same trees.

## Done

- `Source/KeysRun/KeysMangroveEco.h/.cpp` (new): the rules (`PlanTile`: what each waterside tree holds, by its own
  fixed dice), the rookery keys, the regions, the census, `AKeysMangroveEco` (plans a tile within 900 m off the game
  thread, lets it go past 1.3 km), the links and their log lines, the fishing and sound hooks, the test commands.
- `KeysMangroveEcoBirds.cpp` (new): 13 kinds of bird drawn from their pose into two meshes (moving, and still or
  far), wading, stalking, striking, flying, roosting, flushing, rookery flights.
- `KeysMangroveEcoWater.cpp` (new): schools of small fish, predators, sharks, ray, jellyfish, crabs; one instanced
  mesh per kind with the school fish's material.
- `KeysMangroveEcoInt.h` (new): the lane's own shared types.
- `KeysMangrove.cpp`: the ecosystem is spawned from `BuildMangroves` (real map only); the still waders and crab
  specks are left out when it is on; oysters and sponges on the roots below the tide line; a bare limb where a
  cormorant dries; a roost snag on a tile's most open point; `SitesOfTile` / `RootPoint` hand the trees to the rules.
- `-KRNoMangroveEco` leaves all of it out and puts the old still waders back.

## Verifier pass (the `verifier` agent, 11 runs of its own)

- Held: the tide rule (23 in on the flood, 23 out on the ebb), the rush / birds / waders / night link lines, the
  flush (none at idle, 4 birds at 23.8 m/s), rookery flights at dawn and dusk, the classic map, `tpcheck`, the dock
  audit, the same trees at the reference spot, flat and creek frame times, tile arrival, start-up time.
- Did not hold, and corrected above: three link screenshots first cited (`f2/55`, `f1/55`, `f4/51`) did not show
  what was claimed (replaced by the `g1` stills); the rookery perf claim; the chase-view part of criterion 1; the
  "2 m" creek visibility; a zero-count `link night` line at start (now not logged); fiddlers tipped on their sides
  (fixed); 145 against 154 rookery birds (explained).
- Not checked by anyone: the osprey's dive in a still, crabs dropping or burrowing, rookeries other than 7 at
  dawn, manatee haunts outside the Middle Keys, any packaged-build number.

## In progress

- Nothing.

## Blocked

- Nothing.

## Requests for the marshal

- R1 (open): `Source/KeysRun/KeysLife.cpp`, `KeysBoatPawn.cpp` - `PickSpecies` multiplies a species' weight by
  `KeysMangroveEco::PickBoost` (snook, tarpon, jacks, barracuda, small sharks, mangrove snapper where they lie), the
  first bite timer by `KeysMangroveEco::BiteScale` (0.8 or 0.6 where bait is thick), `picks` logs the bait line, and
  the script takes `eco:<command>` (routes to `AKeysMangroveEco::DebugCmd`). Both functions return 1 with no
  ecosystem. patch: `.marshal/requests/mangrovelife/R1.patch` (applies to this branch's base).
- R2 (open): audio - poll `KeysMangroveEco::PopSound` and play it; list of calls and splashes, what the project's
  synth can already do, and what would be wanted from the paid library (needs the user's OK; nothing bought):
  `.marshal/requests/mangrovelife/R2-sound.md`.
- R3 (open, small): `KeysSchools.cpp` Creek mix and `KeysLife` also put silversides, snapper, barracuda and jacks
  under the mangroves; the two do not know of each other. Either is fine alone; together the creeks are a little
  crowded. Suggest lowering the Creek zone density in `ZoneMixes()` from .8 to .4 once this merges.

- R4 (open, a judgement): criterion 1 asks for chase-view shots of each kind. They exist for 5 of 13 kinds; the
  rest are shown only by the lane's test camera. Either accept the test-camera shots, or the birds need to be drawn
  larger or the chase camera brought in at idle speed (a hub change in `KeysBoatPawn`).

## Guesses (the lane's own)

- Night is the feeding clock's hour (19:45 to 05:15), because the game has only day and sunset lighting: after dark
  the birds behave as at night but are lit as day or sunset.
- A rookery key is a wild all-mangrove island of 1,300-14,000 m2 off the road; three per part of the chain.
- The parts of the chain are the map's bake phases along US-1 (p3 Lower, p1 Middle, p2 Upper, p4 Key Largo).
- Birds are drawn 1.5 times life size (kingfisher 1.9, green heron 1.7), as the old waders were drawn large.
- A boat flushes birds above 3.5 m/s inside 12-36 m (by kind), or inside 6 m at any way on; idling does not.
- Each school leaves the roots at its own height of tide between -0.55 and +0.05; all are out at dead low.
- "Pilchard" uses the herring model, "mullet" the bonefish model, juvenile snapper the mangrove snapper model.

## Not right yet (seen, and left)

- In the chase view the birds are small (a wide lens) and the boat hides what is ahead; the close shots above are
  from the lane's test camera (`kr.eco look`), which is not something the player has.
- The oysters and sponges are few (about four lumps a tree): more made the creek's root tiles slow to arrive.
- Under water the predators are hard to make out in the lane's test-camera shots. The surface is not drawn from
  below.
- A cormorant or anhinga drying its wings can look as if it sits in mid-air: its limb is thin and, from the open
  water, end on. Whether the limb reads from the side was not settled. An anhinga's neck can pass through a root.
- The rush, shower and bird lines depend on the hour: with no `clock=` the game takes the real time, and at night
  the terns and waders are asleep. Check them with `clock=12:00`.
- Fiddler crabs are pale; ibis are faint against bright water; the root lumps are flat-coloured diamonds.
- Fish swim through roots; a wader can stand in a root cage; a second or third rookery bird on a crown can sit a
  little into or off the leaves; a kingfisher and a night heron read pale from some sides.
- The osprey's dive, a mullet in the air, the crabs dropping and the fiddlers going down were not caught in stills.
- Manatee haunts outside the Middle Keys were counted but not visited. They still come from `AKeysLife::DebugSpawn`.
- The census takes 0.6-5.5 s on a background thread after start (it is only for the log).

## Notes for integration

- Apply R1 with `git apply`; it makes `KeysLife.cpp` and `KeysBoatPawn.cpp` include `KeysMangroveEco.h`.
- Test commands (console, or `eco:` after R1): `stats`, `census [all]`, `fish`, `links`, `rook`, `shower`,
  `find <kind> [dist]`, `findall [dist]`, `look <kind> [dist] [height] [side] [fov]`, `look rook <n>`,
  `look shower`, `look off`.
- Busiest rookery for the perf bookmarks: `tp=11030,-4734,1.102` (rookery 7, Upper Keys bay side, 154 birds).
- A scripted run launched on a failed build hangs on the rebuild prompt and holds the game lock: this lane did
  that once (its own process, killed by the lane) and held another lane's run up for some minutes.
- The agent types were available; the animals and placement were done in one session following the `sea-life` and
  `world-builder` briefs, and the `verifier` agent was asked for the independent pass.
