# reeflinks

Status: IN PROGRESS
Updated: 2026-11-24 (after the marshal's three points; runs ev4, ev5)
Branch: lane/reeflinks (see `git log`)

The user's words: "behaviour links" (after the marshal's list "cleaning stations, fish guarding territory, angelfish
nipping sponges, parrotfish sleeping in the new holes at night") and the standing rule "i just want to keep it
realistic". The nine criteria are the marshal's reading; what this lane guessed is under "Guesses and decisions".

Evidence: `L` = `/Users/mat/Documents/Unreal Projects/KeysRun-devkit/lanes/KeysRun-reeflinks/` (`L/log_<tag>.log`,
`L/shots/<tag>/KRScript_NN.png`). Runs: `base1` (before), `day1`, `ev2`, `ev3`, `reg1`, `reg2` (classic map).
Every shot named here was opened. `ev2` and `ev3` ran with R2 applied locally (KeysSchools restored before commit).

Not READY FOR MERGE. After the marshal's three points (runs `ev4`, `ev5`): perf A/B done (8, within noise, not
proven inside 3% on GPU); untested items run; pictures still missing: gobies on a client, a sponge bite close up,
a turtle under a ledge, a cave or plate den from above.

## Acceptance criteria
- [ ] 1 Sleeping in the holes - partly. Parrotfish at 22:00 go to the foot of a stony head and sleep in the
  cocoon: `L/log_ev3.log` "sleep: stoplightparrot initial #18 ... wedged in at the foot of a coral head (no empty
  den within 16 m) because the feeding clock reads 10:00 pm ... spinning its mucus cocoon" (also rainbow and
  midnight); shot `ev3/60`, `ev3/61` (stoplight in its cocoon under the rim of a head). In a den (30 m search):
  `L/log_ev4.log` links line "slept 23 in dens, 229 at the foot of a head"; shot `ev4/65` (midnight parrotfish
  head-first in a den's hole at night, tail toward the camera, cocoon round it).
  Turtle rest under a ledge: `L/log_day1.log` "rest: greenturtle adult #266 ... heads for an empty ledge 29.5 m off
  ... because it has fed 317 s since it last rested", then "settled under the ledge at -1154.3, -11.5, 2908.7 ...
  until it needs air, in about 72 s"; again in `L/log_ev4.log`; in `ev5` that turtle found no free ledge within
  40 m ("rest: ... no empty ledge or cave lies within 40 m"). No shot of a turtle under a ledge. Squirrelfish/soldierfish out by night, home by day: `L/log_ev2.log`
  "KRDen night: 8 Blackbar soldierfish ... leave the ledge", "KRDen day: 6 Squirrelfish ... go back into its
  shadow"; shots `ev3/62`, `63` do not show the fish clearly.
- [ ] 2 Territory - rule yes, egg patch not seen. Unasked: `L/log_day1.log` "chase: sergeantmajorguard guard #157
  ... went for queenangel adult #155 ... because it came within 2.3 m of his eggs on the rock", "chase over: ...
  pursued it 6.0 m in 4.0 s, it is now 2.8 m away, he turns back to his eggs"; with `reef:intrude`
  (`L/log_ev2.log`): "went for stoplightparrot terminal #162 ... within 1.9 m". Hoverers over a head:
  `reef:roles` "hoverer: 1 groups, 40 animals ... mean height over the reef 1.90 m, furthest from home 1.3 m",
  shot `ev2/58` (sergeant majors above a head). The egg patch (180 triangles, `KRReef mesh sergeant major eggs`)
  shown from above in `ev5/54`: a purple patch on top of a brain coral head (`reef:look @eggs 0.5 up 1.8`).
  Guard shot `ev2/55` (dark blue male).
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
  alarm: ... effect: 0 reef fish within 3.5 m bolted" (a 20 m den). With the 5 m radius at Sombrero's own green
  moray (6.5 m, `kr.den hunt near`, `L/log_ev5.log`): "noses into the rock ... stop 1 of 3", "link alarm: ...
  0 reef fish within 5.0 m bolted (3 reef animals within 10 m)". The rule fires; no reef fish was within 5 m at
  either stop, so none bolted. Not shown in a shot (`ev5/59`, `60` blocked by a coral head).
  Grouper over the ledge: `ev2/77`, `ev2/81`. Jacks shadow a digging ray (needs R2): "KRSchools link shadow: a
  school of jackcrevalle (160) turns to the southern stingray digging ... 17.6 m off", "... leaves the stingray
  (cause: it has stopped digging)"; ray digging in its cloud `ev2/65`-`67`.
- [ ] 6 The faults - octopus by day only arm tips at the mouth `ev2/70`; nurse shark broader and curved `ev2/71`;
  moray body along the floor and a travelling wave out of the hole (built; `ev2/68` too close to judge); ray lies
  in the drawn sand (`KeysReefDen::FloorNear`) `ev2/64` - flat on the sand, not clearly half buried; pale den
  floor: dens now stand on the highest drawn floor over a 5 x 5 grid under the pocket + 4 cm (my reading of
  `reefdens` molasses/56: sand of the 2 m lattice peaking inside the pocket) - the cave and plate shots `ev2/72`,
  `73` and `ev5/50` are blocked; `ev5/58` (plate, level view) shows a dark opening; the view from above is still
  NOT shown. Breathing turtle 0.2 m lower: `ev4/50`-`52` - the green turtle drawn as it rises to within about a
  metre of the surface in the mask view, `ev4/52` up by the surface ("took air for 5.0 s at the surface").
- [x] 7 The recipe - R1 (section 7 written from the reef lanes' status files, the link traits, the new hooks).
- [ ] 8 No slowdown - within noise, not proven inside 3%. Diagnosis (`L/log_ev5.log`, dive at 6 m, `goto=reef`,
  Medium, back to back, 15 s settle after each switch; `reef:links 0` deals everything again without the neon
  gobies, egg patches or links, i.e. the "before"): on game 17.14 draw 14.95 gpu 24.29; off 17.94 / 15.64 /
  22.61; on 17.74 / 16.28 / 22.34; off 17.50 / 15.73 / 22.48; on 18.45 / 18.36 / 24.76. Means on 17.78 / 16.53 /
  23.80, off 17.72 / 15.69 / 22.55: game +0.3%, draw +5%, gpu +5.5%, but the on samples straddle the off ones
  (on2 is below both offs on gpu). `ev4` gave the same picture with more spread (off2 draw 18.75 = on2). The
  +6 ms draw from the first after-runs was the busy machine: today's "off" dive draw 15.6-15.7 matches `base1`
  15.55. What is new and drawn: 3 goby pools (tiny, 936-triangle near mesh only within 3.3 m) and one egg pool
  (180 triangles, 1-2 instances, no shadow); pools written per frame 19-25 on or off. To be sure inside 3%: the
  marshal's quiet or packaged A/B with `-KRNoReefLinks` (new switch). Older numbers: Medium, `goto=reef`: boat before (`base1`) game 11.93 draw 7.22 gpu 31.14,
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
  (retries 40 s; picks the clearest line of sight of 12 bearings; `up <m>`), `reef:links 0|1` and
  `-KRNoReefLinks` (perf A/B), `links:` line in `reef:stats` (with pools written a frame), `kr.den hunt near`
  (the nearest green moray gets a grouper, test hook); the test camera holds a cleaned client and a sponge biter.
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
- Shots: gobies on a client (`reef:look @clean` lands too late or the client is lost in the frame), a sponge
  bite close up (`ev5/55`, `56` show no fish), a turtle under a ledge, a cave and a plate den from above.
- Reef fish bolting from the moray hunt (rule fires; none were within 5 m).

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
