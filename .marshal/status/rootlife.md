# rootlife

Status: BLOCKED (perf retake waits for the user's packaged game to close: KeysRun-Mac-Shipping pid 23172 still running at the last check); criterion 5 partly met
Updated: 2026-11-24 evening
Branch: lane/rootlife (see git log; last code commit e2060ff)

Evidence lives in `/Users/mat/Documents/Unreal Projects/KeysRun-devkit/lanes/KeysRun-rootlife/` (shots/<tag>/, log_<tag>.log). Scripts: shots/p2 is the proof run
(script in the log's command line), pfA*/pfB* the frame-time pairs (A root life on, B `-KRNoRootLife`), cls the classic map.

## Round 2 (the marshal's three points)
1. Growth as a crust - done (2c2a7f0 and the next commit): every piece now sits on one side of the root, small (oyster clumps
   of three rounded shells lying on the bark, 3-10 cm; fire sponge as three irregular lobes and a vent; tunicates as five-bead
   bunches; algae as small tufts; barnacles a thin pale band 4-10 cm long; anemones 5-8 cm), scale 0.6-1.15 of the root,
   rotated all round the root (about half toward the open water), one per station every 5-12 cm, two per station and larger
   below low water (thickening toward the bottom), bark between. Meshes 16-116 triangles (were 56-156). Same budget: 26 m,
   cap 3,600 pieces (more pieces, smaller), ~190k triangles in Sister Creek (was ~180-220k). Evidence:
   shots/p4/KRScript_54.png (snook in the roots with the crust round them), p4/_51, _52, p3/_50.._55. Still low-poly; the
   sponge close look in p4/_50 is blocked by the bank.
2. Walls by rule - the census now needs 1 m+ of water at high tide (0.7 m charted at 3 m out, 0.9 m at 6 m), 8+ trees and fish
   within 15 m, and ranks by crowd (creek banks +1.5); eco:dive roots also counts trees within 8 m and gives +2.5 for snook or
   gray snapper within 10 m. Lower Keys (-9096,-394, off a mangrove key): 20 trees within reach, snook in frame p4/_56, gray
   snapper p4/_57, mask view p4/_01. Key Largo (now 30296,-10181, off a mangrove key): shots/kl/_00 (mask, half above the
   surface: thin water), _50, _51 (silversides and pilchards in the roots), _52 (gray snapper, small in frame), _53 (growth);
   only 7 trees within reach and no snook near: still thin. The map's Key Largo fringe here is sparse; I did not find a
   creek bank there.
3. Perf - NOT RETAKEN: the user's packaged game (KeysRun-Mac-Shipping) was still running through all of round 2. The numbers
   below (round 1) stand until then. To take: the pfA/pfB script in this file's criterion 6, diver spot dv:at=-265.1,432.2.
   Also noted in round 2: tile hitches after long teleports of 68 and 91 ms (log_p3, log_kl), with the user's game running;
   not compared against -KRNoRootLife yet (roots now sample the bottom height per root: a possible small cost).

## Acceptance criteria (the marshal's reading; my guesses at the end)
- [~] 1. Thick schools in the roots - done in code, partly shown. Schools now follow the roots by depth: on every creek-bank tree
  one in two holds a school (was one in three of a third of them), fringe trees by the water off them (channel edge .32, fair
  .2, shallow flat .09), none where there is never 0.4 m at high water, and they leave the roots when there is under 0.4 m.
  Silversides 140, pilchards 90, juvenile gray snapper / grunt 34, juvenile schoolmaster and lane snapper (new kinds) 26, each
  x0.6-1.3; sergeant majors (new kind, own model) at the edge; a band along the root line, the silversides and pilchards
  weaving through it, the small snappers and grunts hanging head to the current; the fish part round a diver one by one
  (no longer the whole school scattering). Sister Creek, 600 m round: 355 schools planned (240 before), 73 awake (35).
  Evidence: shots/p2/KRScript_03.png (from the mask: pilchards and silversides through the roots), p2/_50.._60 (each kind),
  log_pfA3.log "KREco drawn". Not good enough yet: Lower Keys and Key Largo walls are thin (see 5).
- [~] 2. Growth on the roots - done (KeysMangroveEcoRoots.cpp): 8 instanced meshes (barnacle crust, oyster cluster, red and
  orange fire sponge, purple and blue tunicates, green algae fuzz, anemone) laid along the roots exactly as drawn
  (RootPieces), banded by height (barnacles in the top 0.2 m of the tide, oysters through the intertidal, sponge, tunicates,
  algae, anemones below low water), only below 0.32 m (high water) and above the bottom; creek banks 0.5 cover, deep fringe
  0.42, shallow 0.18; within 26 m of the camera, relaid when it moves 7 m (0.7-1.0 ms), drawn to 24 m, cap 1,700 pieces
  (~180k triangles). Nothing in the tile mesh: the old oyster lumps are gone from it (tile 74,846 triangles against 75,934).
  Evidence: shots/p2/_61.._68 (close shots), p2/_03, "KRRoot stats" lines. Weak: the barnacle and oyster close shots at
  tide=1 are at the surface (half above, dark sky: p2/_66, _67); the shapes are low-poly and read as sleeves/blobs.
- [x] 3. Snook and snapper where they can be seen - snook 1-5 under the outer roots (creek bank 8.5% of trees), lying
  near the bottom side by side, nose along the shore (turns with the tide); adult gray snapper (new kind GraySnapper,
  5-14) just inside the root line, drift about their places, face a diver within 7 m and back into the roots as he comes
  (2.5 m: well in). Barracuda hangs off the edge; tarpon 1-2 in a creek. Evidence: p2/_50 (snook side-on in the roots),
  p2/_51 (gray snapper), p2/_59 (barracuda), p2/_60 (tarpon, far). Not shown: the snapper turning to a diver (no shot).
- [x] 4. A camera that can show it - every eco:look under the surface goes through SafeEye: moved out until there is water
  to hold it, never under the bottom, backed out of any root, at least ~0.9 m under the top of the tide where the water
  allows (closer than that the picture splits at the surface, found by shots); in thin water it looks along. New:
  `eco:look roots` (best root wall within 160 m, seen along it), `eco:look <growth name>` (barnacles, oysters,
  sponge red/orange, tunicates purple/blue, algae, anemone), `eco:dive roots [dist]` (puts the diver off the best wall,
  held at depth, looking along it), `eco:gowall <0-3>` (boat to a region's best wall from the census; census prints
  "KREco where <region> / root wall"). Evidence: log_p2.log "KREco dive roots", p2/_03, _04, _05. Remaining: subjects at
  the top of the tide are still taken half above the surface (by rule).
- [~] 5. Proved - Sister Creek at tide=1, clock 10:30 done: p2/_03 (wall from the mask: schools, growth, roots), close
  shots of every fish kind and growth kind (p2/_50.._68). Side by side with the marshal's: shots/sbs/sbs_05_east.png,
  sbs_06_west.png, sbs_07_north.png (same script steps; the diver does not land in the same spot as in the marshal's run,
  so the views differ), sbs_wall.png (marshal _05 against my wall shot). Lower Keys (wall off a mangrove key at -9096,-394,
  p2/_04, _69.._71) and Key Largo (off Rattlesnake Key, 28857,-7962, p2/_05, _72.._74): taken, but thin: 21 and 7 trees within
  reach, few schools, no snook in frame. My honest reading: Sister Creek now looks busier and the roots carry visible life,
  but it does not yet read as a crowded nursery; the other two regions do not.
- [ ] 6. No slowdown - NOT MET. Medium, 1600x900, editor, the user's packaged game was running for part of it (frame
  times noisy; take as indicative). Diving in Sister Creek, fixed diver spot (dv:at=-265.1,432.2, hold 1.2, look -2.599,0):
  before (-KRNoRootLife, pfB1+pfB2, 4 samples) game 17.0-18.3 (mean 17.4), draw 16.2-17.7 (16.8), GPU 28.7-30.8 (29.6);
  after (pfA3, last build) game 18.1-19.2 (18.6, +7%), draw 17.4-18.4 (17.9, +6%), GPU 30.9-31.6 (31.2, +5%). pfB3's dive
  was disturbed (draw 32-34) and is left out. Flat off Boot Key from the boat: no difference (game 11.2-11.8 both, GPU
  34.2-34.4 both, capped). Sister Creek from the boat: after is LOWER, every pair (GPU 29.8-30.2 against 35.3-37.0, draw
  10.4-11.7 against 30.1-35.9) - unexplained; I did not chase it. eco:stats in the dive: instances 1,540 (244k tri) against
  1,217 (156k), plus growth 1,698 (181k tri); eco cpu 0.92 ms against 0.72-0.82. KRMangrove tile: the only line over 50 ms is
  the first tile at start-up, 400 ms, the same with and without (log_pfB1 too, and in base0 on the old code); tiles arriving
  later 42-45 ms (log_p2). What I cut: growth radius 44->26 m, cap 7,000->1,700, meshes 80-324 -> 56-156 triangles; fish
  drawn all within 10 m, one in twenty at 45 m, none past 50 m (the rest of a school waits unmoved); schools awake to 80 m
  (was 105). Next cuts if the marshal wants them: growth to 18 m, fish full only within 6 m.
- [x] 7. Nothing else moves - tpcheck 48 destinations 0 bad (every pf log), classic map 19, 0 bad (log_cls.log, and no KREco
  or KRRoot line there: nothing of mine is built); dk:audit 545 docks, 0 overlaps (log_pfA1/A2); trees from the boat at
  tp=-1150,452,4.71: shots/sbs/sbs_trees_boat.png (before/after: the same; the oyster lumps that left the tile mesh are not
  visible from there). Note: with root life on, prop roots now reach down into the bottom where it is deeper than their old
  foot (no roots ending in mid-water over a creek channel): a change under water only.

## Done
- KeysMangroveEcoRoots.cpp (new): RootLife() switch, growth meshes and instances, PlanRootLife (fish rules), FindWall, SafeEye.
- KeysMangroveEco.h / EcoInt.h: new kinds Schoolmaster, LaneSnapper, Sergeant (schools) and GraySnapper (adults); FSite.bThird;
  SitesOfTile(..., bAllBank); RootPieces; growth pools; tile tree lists; census walls.
- KeysMangroveEcoWater.cpp: band-along-the-roots schools, distance thinning, diver parting, hanging fish, lying snook/snapper/barracuda,
  sergeant major model, new pool specs.
- KeysMangroveEco.cpp: PlanTile uses PlanRootLife for every bank and fringe tree (birds and the rest keep the old one-in-three);
  eco:look roots / growth names, eco:dive roots, eco:gowall, stats line "KRRoot stats"; BiteScale divides the bait count by 4
  under root life so the bite keeps its old pace.
- KeysMangrove.cpp: no oyster lumps in the tile mesh under root life; roots reach the bottom; RootPieces; tile hitch logged
  over 40 ms (was 60) so the 50 ms rule can be read.

## Requests for the marshal
- None. (No hub file needed: everything is behind existing eco: commands.)

## Notes for integration
- Off switch: -KRNoRootLife gives the ecosystem exactly as before (lumps in the tile mesh, old schools and snook).
- Enum EKind grew (new kinds inserted after Grunt and after Tarpon): anything outside my files that stores kind numbers would
  need a look; I found none (KeysAudio takes them through PopSound at run time).
- The marshal's "same view as mshots2" could not be reproduced exactly: the diver lands elsewhere after dive with the same steps.

## Guesses I made (the criteria are the marshal's reading)
- Tidal range taken as KeysGeo's 0.32 m either side; barnacle band = top 0.2 m, subtidal = below -0.26 m.
- Growth proportions and colours from general knowledge of Keys red-mangrove fouling (Tedania ignis, colonial tunicates,
  Isognomon / Crassostrea, Aiptasia/Bartholomea), not from a reference photo.
- School sizes, probabilities, snook/snapper group sizes and the "0.4 m to hold in the roots" are my numbers.
- Juvenile schoolmaster/lane use the adult models scaled down (no juvenile colouring); the sergeant major is my own model.
