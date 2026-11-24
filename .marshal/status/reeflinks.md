# reeflinks

Status: IN PROGRESS
Updated: 2026-11-24 18:20
Branch: lane/reeflinks (see `git log`)

The user's words: "behaviour links" (after the marshal's list "cleaning stations, fish guarding territory, angelfish
nipping sponges, parrotfish sleeping in the new holes at night") and the standing rule "i just want to keep it
realistic". The nine criteria are the marshal's reading; what this lane guessed is under "Guesses and decisions".

Evidence: `L` = `/Users/mat/Documents/Unreal Projects/KeysRun-devkit/lanes/KeysRun-reeflinks/` (`L/log_<tag>.log`,
`L/shots/<tag>/KRScript_NN.png`). Runs: `base1` (before), `day1`, `ev2`, `ev3`, `reg1`, `reg2` (classic map).
Every shot named here was opened. `ev2` and `ev3` ran with R2 applied locally (KeysSchools restored before commit).

Not READY FOR MERGE: several criteria have the rule working in the log but no good shot, and one perf number is
not settled (see 8).

## Acceptance criteria
- [ ] 1 Sleeping in the holes - partly. Parrotfish at 22:00 go to the foot of a stony head and sleep in the
  cocoon: `L/log_ev3.log` "sleep: stoplightparrot initial #18 ... wedged in at the foot of a coral head (no empty
  den within 16 m) because the feeding clock reads 10:00 pm ... spinning its mucus cocoon" (also rainbow and
  midnight); shot `ev3/60`, `ev3/61` (stoplight in its cocoon under the rim of a head). NOT seen: a parrotfish in
  a den - at Sombrero no empty den lay within 16 m of any of them; the search is now 30 m (built, not run).
  Turtle rest under a ledge: `L/log_day1.log` "rest: greenturtle adult #266 ... heads for an empty ledge 29.5 m off
  ... because it has fed 317 s since it last rested", then "settled under the ledge at -1154.3, -11.5, 2908.7 ...
  until it needs air, in about 72 s"; no shot. Squirrelfish/soldierfish out by night, home by day: `L/log_ev2.log`
  "KRDen night: 8 Blackbar soldierfish ... leave the ledge", "KRDen day: 6 Squirrelfish ... go back into its
  shadow"; shots `ev3/62`, `63` do not show the fish clearly.
- [ ] 2 Territory - rule yes, egg patch not seen. Unasked: `L/log_day1.log` "chase: sergeantmajorguard guard #157
  ... went for queenangel adult #155 ... because it came within 2.3 m of his eggs on the rock", "chase over: ...
  pursued it 6.0 m in 4.0 s, it is now 2.8 m away, he turns back to his eggs"; with `reef:intrude`
  (`L/log_ev2.log`): "went for stoplightparrot terminal #162 ... within 1.9 m". Hoverers over a head:
  `reef:roles` "hoverer: 1 groups, 40 animals ... mean height over the reef 1.90 m, furthest from home 1.3 m",
  shot `ev2/58` (sergeant majors above a head). The egg patch (180 triangles, `KRReef mesh sergeant major eggs`)
  is drawn by the log ("4 egg patches alive (2 drawn)") but no shot shows it: `ev2/55` and `ev3/54` are blocked
  by elkhorn. Guard shot `ev2/55` (dark blue male).
- [ ] 3 Cleaning stations - rule yes, reef-life shot missing. `L/log_day1.log` (unasked but one `reef:clean`):
  "clean: stoplightparrot initial #246 ... makes for the cleaning station ... because it has gone 123 s since it
  was last cleaned", "queue: ... is being cleaned; effect: it waits its turn off the head, 1 in the queue",
  "holds still head-down at 35 degrees, rolled on its side, fins spread, for 11 s ... the 2 neon gobies of the
  station come off the head and pick it over", "clean over: ... next in the queue, comes in"; 24 cleanings and 24
  waits in 200 s, clients stoplight, rainbow, midnight, queen, French, gray angelfish. A waiting client: `ev3/55`.
  The dens' shrimp station with its queue: `L/log_ev2.log` "a young black grouper comes to the banded coral
  shrimp's station ... but a grunt is being cleaned ... it waits its turn a metre off", shot `ev2/75` (grunt
  head-up at the shrimp's hole). No shot yet of gobies on a client (the look camera arrived late or was blocked).
- [ ] 4 Angelfish nip sponges - rule yes, shot weak. `L/log_day1.log` "nip: queenangel adult #31 ... nipped at
  the wall of a tube sponge ... because angelfish feed on sponges"; 143 nips in 200 s, 161 in `ev2`. Shots
  `ev3/52`, `ev3/53`: French angelfish beside the pink tube sponges, the bite itself not caught.
- [ ] 5 Hunters together - yes in the log, shots weak. `L/log_ev2.log`: "KRDen link hunt: a black grouper comes
  to the green moray's hole", "the grouper shakes its head", "the Green moray noses into the rock at ... (stop 1
  of 3, 2.5 m from its hole) while the grouper waits over it ... effect: the small fish near bolt", "KRReef link
  alarm: ... effect: 0 reef fish within 3.5 m bolted" (a 20 m den: no reef fish there; radius now 5 m, not run).
  Grouper over the ledge: `ev2/77`, `ev2/81`. Jacks shadow a digging ray (needs R2): "KRSchools link shadow: a
  school of jackcrevalle (160) turns to the southern stingray digging ... 17.6 m off", "... leaves the stingray
  (cause: it has stopped digging)"; ray digging in its cloud `ev2/65`-`67`.
- [ ] 6 The faults - octopus by day only arm tips at the mouth `ev2/70`; nurse shark broader and curved `ev2/71`;
  moray body along the floor and a travelling wave out of the hole (built; `ev2/68` too close to judge); ray lies
  in the drawn sand (`KeysReefDen::FloorNear`) `ev2/64` - flat on the sand, not clearly half buried; pale den
  floor: dens now stand on the highest drawn floor over a 5 x 5 grid under the pocket + 4 cm (my reading of
  `reefdens` molasses/56: sand of the 2 m lattice peaking inside the pocket) - the cave and plate shots `ev2/72`,
  `73` are blocked, NOT verified; breathing turtle 0.2 m lower (guess at the surface fault) - NOT verified.
- [x] 7 The recipe - R1 (section 7 written from the reef lanes' status files, the link traits, the new hooks).
- [ ] 8 No slowdown - not settled. Medium, `goto=reef`: boat before (`base1`) game 11.93 draw 7.22 gpu 31.14,
  after (`reg1`) 11.81 / 7.26 / 31.56 (gpu +1.3%), (`day1`) 12.61 / 8.17 / 32.01 (+2.8%). Dive 6 m before
  frame 33.55 game 18.01 draw 15.55 gpu 25.96; after (`day1`) 33.79 / 22.59 / 21.82 / 26.30, (`reg1`, uncapped
  that run) 21.92 / 19.44 / 21.87 / 21.17. The draw thread reads 6 ms higher in both dives after; other lanes
  were building and running beside it; one "before" only. Reef life's own cpu 0.10-0.15 ms before, 0.10-0.24
  after; dens 0.009-0.017 ms. Animals: 254 before, 281 after (30 neon gobies cap, 0.25 units each). The goby's
  near mesh (936 triangles) is used only within 3.3 m. Needs the marshal's quiet A/B.
- [x] 9 Nothing else moves - `L/log_reg1.log` "KRTp checked 48 destinations, 0 bad", "KRDocks audit: 545 docks;
  ... another dock 0", "KRDive speared goliath 163.6 lb at 4.0 m" (straight after diving); `L/log_reg2.log`
  (classic) "KRTp checked 19 destinations, 0 bad", reef life 218 animals; day and night differ as the rules say
  (`L/log_ev3.log` 10:00 pm sleep lines; `L/log_ev2.log` KRDen night/day lines).

## Done (files)
- `KeysReefLife.h/.cpp`: traits `bSleepsInHoles`, `bSponges`, `bClient`, `bCleaner`, `RestEvery`, `bEggs`,
  `bShadows`; cleaning states and stations; dens taken for sleep and rest; egg-patch pool; `Alarm`,
  `DiggingRays`; guard chases any fish under 4x his length (not turtles, rays, bottom animals, cleaners);
  hoverers home on a head; chased intruder runs 3 s; ray and pits on `FloorNear`; dig 10-20 s; tiny fish LOD;
  hooks `reef:clean`, `reef:intrude`, `reef:do clean|rest`, `reef:look @clean|@wait|@nip|@rest|@den|@chase|@eggs`
  (retries 40 s), `links:` line in `reef:stats`.
- `KeysReefLifeGoby.cpp`: neon goby, Elacatinus oceanops (new; the cleaner).
- Species files: parrotfish sleep in holes and are clients; angelfish on sponges and clients; guard has eggs;
  turtles rest; ray `bShadows`.
- `KeysReefDen.h/.cpp`: `FloorNear` public; den Y over the pocket grid. `KeysReefDenLife.cpp`: moray poses and
  wave, hunt stops at coral feet + `Alarm`, octopus by day, nurse shark, shrimp-station queue with a grouper.

## Requests for the marshal
- R1 (open): `.claude/skills/reef-creature/SKILL.md` - section 7, link traits, hooks - `.marshal/requests/reeflinks/R1.patch`
- R2 (open): `KeysSchools.cpp` - jacks and blue runners within 40 m of a digging southern stingray come in over
  it (`KeysReefLife::DiggingRays`), `KRSchools link shadow` lines - `.marshal/requests/reeflinks/R2.patch`
  (applied locally in `ev2`, `ev3`, `reg1`: works as logged above)
- R3 (open): the dive perf A/B on a quiet machine (draw thread +6 ms in two noisy runs).

## Left to do
- Shots: gobies on a client, a sponge bite, the egg patch, a turtle under a ledge, a parrotfish in a den (30 m
  search), the cave/plate floor, a turtle at the surface.

## Guesses and decisions (my own)
- Neon goby as the cleaner (a Florida reef tract fish); juvenile blue tang are not cleaners, so not used.
- Clients go every 90-420 s by day; held 8-18 s, head-up 40 or head-down 35 degrees, rolled 0.5 rad; a queue of
  two; stations on stony heads only.
- Sergeant major guard chases fish up to 4x his length (the species' reference block said "near his own size";
  real guards rush parrotfish); eggs only on brain, star or den rock, on its top.
- Midnight and rainbow parrotfish now sleep in a cocoon (reeffish had them without; many Scarus do).
- Turtles rest about every 420 s of feeding, 9:00-16:30, under an empty ledge or cave within 30 m.
- Rays dig 10-20 s (was 5) so followers can reach them; jacks/blue runners as the followers (bar jacks do this;
  the game has crevalle jacks and blue runners).
- Den stand on the pocket's highest drawn floor + 4 cm; breathing turtle 0.2 m lower: both guesses at causes.
