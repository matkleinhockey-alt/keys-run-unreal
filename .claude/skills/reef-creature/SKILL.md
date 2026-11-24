---
name: reef-creature
description: Add one reef species to Keys Run's reef-life framework (KeysReefLife) in its own file, and prove it. Use for any fish or other reef animal that is an individual on the reef rather than a boid school or a spear target.
---

# Adding a reef creature

A species is one new file, `Source/KeysRun/KeysReefLife<Group>.cpp`. You never edit `KeysReefLife.h`,
`KeysReefLife.cpp`, `KeysReefLifeBody.cpp` or another species' file. Copy `KeysReefLifeParrot.cpp` (one species,
two phases, every trait) or `KeysReefLifeTang.cpp` (the shortest complete one) and change it. Several species of
one group may share a file; each gets its own `MakeXxx` and its own `FAuto` line.

## 1. Write down the animal first

Put a reference block at the top of the file before any code: real common and scientific name, length range in
cm for each phase or life stage, colours part by part (body, belly, each fin, eye, marks), body shape, where it
lives (depth, zone), how many together, what it does by day and at night. The verifier reads your shots against
this block. No invented species, no real species outside the Florida Keys.

## 2. The file

```cpp
#include "KeysReefLife.h"
namespace KeysReefLifeTang            // your own namespace: the build merges .cpp files, bare helpers collide
{
	using namespace KeysReefLife;
	static FSpecies MakeBlueTang() { FSpecies S; /* fill in */ return S; }
	static KeysReefLife::FAuto GBlueTang(&MakeBlueTang);   // the registration (write the namespace out: a bare FAuto is ambiguous)
}
```

Every field of `FSpecies` (defaults in `KeysReefLife.h`):

| Field | Meaning |
|---|---|
| `Key` | lower case, no spaces; the name the test commands take. Must be unique. |
| `Common`, `Scientific` | the real names, as a field guide prints them |
| `Role` | one of the six behaviours (section 4) |
| `Phases` | one `FPhase{Name, LenMin, LenMax, Body}` per colour phase or life stage, lengths in metres |
| `Paint` | `(Phase, FPaint)` -> linear colour of one vertex (section 3) |
| `Mixes` | kinds of group: `Weight` (how often), `AMin..AMax` of phase `A`, optionally `BMin..BMax` of phase `B`. Phase `A` is member 0, the leader |
| `Depth[4]` | depth trapezoid in metres: none above [0], full from [1] to [2], none below [3] |
| `Zones` | mask of `ZCrest ZTop ZSpur ZPatch ZFore ZWall ZSand ZGrass ZHard`. Anything not listed never gets one |
| `GroupsPerHa` | groups on a hectare of perfect habitat (depth 1, full reef cover) |
| `MaxAlive`, `Weight` | the species' own cap on individuals, and budget units each costs (section 6) |
| `Habit` | `Day` (sleeps from 19:15 to 6:00 by the feeding clock), `Night` (the reverse) or `Both` |
| `Shy` | metres kept from a slow diver; it keeps up to 2.6 times that from a fast one and bolts at under half |
| `Cruise`, `Burst` | everyday and fright speed, m/s |
| `Roam` | how far from home the group ranges, m (a Cruiser's circuit radius) |
| `Alt[2]` | height over the reef top it swims at, m |
| `Spacing` | room between group members, in body lengths |
| `Cull` | not drawn past this many metres (60 is plenty: the water hides more) |
| `bRows` | swims with its pectoral fins (wrasses, parrotfish, tangs, damsels): near fish get two rowing fins, the tail stays still |
| `bBitesRock`, `BiteEvery` | grazes: tips onto stony coral or reef rock and bites, about every `BiteEvery` s at 14:30, slower early and late |
| `ScarCm`, `PuffEvery` | a bite leaves a pale scar that wide; a puff of white sand about that often (parrotfish only) |
| `bCocoon` | sleeps in a mucus cocoon (parrotfish, some wrasses) |
| `ChaserPhase`, `ChaseR` | that phase, as leader, drives off another group's leader of the same phase inside `ChaseR` m; for a Territory holder `ChaseR` is the territory |
| `BiteSound` | fills one-shots for `AKeysAudio::Burst` and a sentence saying exactly what they are; played within 9 m of the diver |
| `FirstSight` | the toast the first time the diver sees one (count kept under `reef.seen.<Key>`) |
| `bSleepsInHoles` | at night goes to an empty den (`KeysReefDen`) within 16 m and sleeps in its opening; else wedges in at the foot of a stony head (parrotfish) |
| `bSponges` | with `bBitesRock`: roves from sponge to sponge and nips at their walls from the side; rock only now and then where none is in reach (angelfish) |
| `bClient` | by day, every few minutes, goes to the nearest cleaning station, waits its turn and holds still at an odd angle, fins spread, while the cleaners work it |
| `bCleaner` | with `bHomeOnHead`: its home head is a cleaning station; it works over the client (neon goby) |
| `RestEvery` | > 0: after about this many seconds of feeding by day it rests under an empty ledge until it needs air (turtles) |
| `bEggs` | a Territory holder guarding eggs: home on a massive head, the egg patch drawn on its top, chases any fish under 4 times its length off |
| `bShadows` | a Sitter that digs: listed by `KeysReefLife::DiggingRays` for the fish that follow it |

## 3. Body and colour

`FBody` is the shape in body lengths (the whole fish is 1 long): `HalfH`, `HalfW` (half depth and width),
`Peak` (where it is deepest), `Blunt` and `NoseR` (head), `Ped`, `PedW` (tail base), `TailLen`, `TailH`,
`TailFork` (+ crescent, - rounded), dorsal `Dor0 Dor1 DorH`, anal `An0 An1 AnH`, pectoral `PecU PecV PecLen PecW`,
pelvic `PelU PelLen`, eye `EyeU EyeV EyeR`. Measure them off a side-on photograph: depth over length gives
`2 * HalfH`. Phases may differ in shape (`Terminal.Body.TailFork = .32`).

`Paint` is called once per vertex with `FPaint`: `Part` (`PBody PDorsal PAnal PTail PPectoral PPelvic PEye`),
`U`, `V`, the grid place `I`, `J`, `Side` and `Lod`. On the body `U` runs 0 at the snout to 1 at the tail base,
`V` -1 at the belly to 1 on the back. Use `Hex(0xrrggbb)`. Rules that cost me a rebuild each:
- A mark thinner than the grid vanishes: the near body has 24 stations and 16 round. Test with a tolerance
  (`FMath::Abs(At.U - .25) < (At.Lod == 0 ? .02 : .045)`), not an exact value.
- A per-scale pattern (`(At.I + At.J) & 1`) only at `Lod == 0`; return its mean colour at Lod 1 and 2, or the far
  fish flickers.
- Paint by `U`/`V`, never by `I`/`J`, for anything that must sit in the same place at every level of detail.

## 4. Roles

| Role | What it does | Use for |
|---|---|---|
| `Grazer` | group roves from head to head inside `Roam`; each keeps its place round the leader | parrotfish, tangs, grunts feeding |
| `Pair` | member 0 roves, its mate follows a length behind | butterflyfish, angelfish |
| `Territory` | holds `ChaseR` round home, darts at any animal under 1.5 times its length that enters | damselfish |
| `Hoverer` | hangs over its home head facing the current, drops onto it when a diver nears | chromis, hamlets, basslets |
| `Sitter` | lies still on the bottom, hops to a new perch now and then, sooner with a diver close | hawkfish, blennies, lizardfish |
| `Cruiser` | circles home at `Roam` in mid-water, in file | jacks, rays, a reef shark |

All roles already keep off the diver, bolt, steer round coral, stay under the surface and sleep by `Habit`. If no
role fits, do not branch on a species key in the framework: write the request (a new `ERole` or a new trait
flag, the behaviour as a function of hour, diver distance and neighbours) and use the nearest role meanwhile.

## 5. Test and prove

The marshal must have applied the hub hook (`reef:` commands). Build, then one scripted run on scuba:

```bash
"/Users/mat/Documents/Unreal Projects/KeysRun-devkit/build.sh" && "/Users/mat/Documents/Unreal Projects/KeysRun-devkit/shoot.sh" tang \
 "3:play;4:gfx=1;6:goto=reef;14:reef:census 300;15:reef:stats;16:own=dive.scuba.set;17:dive;19:dv:mask;22:reef:look bluetang adult 1.5;24:shotclean;26:reef:find bluetang;27:reef:look off;40:reef:stats;41:quit"
```

- `reef:census 300` deals every chunk within 300 m without spawning: counts by phase, depth band and zone. Zero
  must stand under every zone you did not list.
- `reef:find <key> [phase]`: the nearest one, its length, depth, height over the reef, state, speed.
- `reef:look <key> [phase] [metres]`: puts the diver abeam of the nearest one and holds the mask on it; its
  group does not shy until `reef:look off`. The fish holds still for 3 s: take the shot 1.5 to 2.5 s after.
- Links between animals (`bClient`, `RestEvery` ...): `reef:do clean|rest` makes the looked-at animal due now;
  `reef:clean` makes the client fish nearest a station due; `reef:intrude` brings a group into an egg guard's
  ground. `reef:stats` prints a `links:` line with the counts.
- `reef:stats`: individuals, instances, triangles, cpu ms a frame. `reef:pins 1` marks every individual for a
  top-down shot. `reef:do bite|puff` and `reef:rival` ask the looked-at fish to act. `clock=22:00` for night.
- Evidence for the status file: the registry line (`KRReef registry`), `KRReef mesh` lines (triangles per level
  of detail), a side-on shot of each phase within 2 m that you opened and compared with your reference block, the
  census lines at Sombrero (`goto=reef`), one other named reef and `-KRClassicMap`, and the cause-and-effect lines
  (`KRReef flee`, `chase`, `sleep`, `wake`, `crunch`, `puff`) for whatever your species does.

## 6. Budget

The system holds 900 units alive (`individuals x Weight`) within its 64 m bubble; the nearest chunks are dealt
first. `Weight` is 1 for a fish up to about 50 cm, 3 for anything drawn with over 1,500 triangles up close or
longer than a metre. Set `MaxAlive` to what `reef:census 60` shows at the densest reef plus a quarter, not higher:
when the caps of all species sum past 900 every species is thinned by the same factor (the registry line prints
it). Keep the near mesh under 1,000 triangles (`KRReef mesh`); the far one is 70. Report `reef:stats` before and
after, and run `perf-check` at `goto=reef` from the boat and in a dive.

## 7. Mistakes already made (do not repeat)

Collected from the reef lanes (`reefparrot`, `reeffish`, `reefvisitors`, `reefbottom`, `reefdens`, `reeflinks`).

**The body**
- `FBody::Arch` (how much more the back curves than the belly) is a field too; measure it with the others.
- Fin paint has two rows only, root and edge (`V` 0 and 1): a band in the middle of a fin cannot be painted.
- A body that is not a fish needs `Build` or `Mesh`; the registry still demands a `Paint` even then (return a
  plain colour).
- A limb (`Limb`, the Kind 3 mesh) is drawn on both sides by turning it, not mirroring it: build it the same fore
  and aft (a ray's outer wing, a turtle's flipper) or one side is backwards.
- `Weight` has no value between 50 cm and 1 m: the 50-90 cm parrotfish use 2. Say which you chose.

**Where it is**
- The bottom as drawn is the seafloor mesh's 2 m lattice (`KeysReefDen::FloorNear`), which over a rise is lower
  than `KeysSea::SeafloorHeightAt`. Anything that lies in or on the sand by its own height must use `FloorNear`
  (the stingray lay 3 cm "into" the bottom by the numbers and wholly on top of it in every shot).
- `TopAt` is a 1 m tile of the highest thing there: a fish that must get close to a coral's side (a sponge's wall,
  a den's opening, the foot of a head) is lifted onto its top unless it is `bFloorOnly` for that stretch. Clear
  the flag after; a fish left floor-only swims through every head.
- A den's rock stands in the coral list as a head (`FInst.Den` = 1): it counts for steering and for "nearest
  head", and its top is the rock's top, not a coral's.
- `HabitatAt` and `KeysReef` disagree on where the reef starts (between DZ 1100 and 1250 the framework says
  "patch reef" where the reef plants turtle grass and no coral): a patch-reef species can be dealt on grass there.
- The head table gives one radius per coral kind, so "at the foot of a head" is rough for wide-topped corals.

**Behaviour**
- Do not stand one behaviour in for another and move on (the angelfish nipped rock for want of a sponge bite):
  write the request, or the trait, and say in the status file what the stand-in is.
- A Sitter lying still is reported as "roving" by `reef:find`; `reef:do bite` is what makes a Sitter hop.
- A behaviour shown with `reef:do` proves the act, not the rule: also show it happening unasked (a log line with
  its cause from a run that did not ask), or bring the animals together with a hook that only places them
  (`reef:rival`, `reef:intrude`, `reef:clean`) and let the rule do the rest.
- A turtle within about a metre of the surface is not drawn in the mask view (open): do not claim anything about
  the surface from a mask shot.

**The test camera and the shots**
- `reef:look` puts the diver level with the animal: edge-on for a ray (follow with `dv:depth` to look down),
  often on the shaded side (add `flip`), sometimes under a ledge or a wreck; she cannot get lower than 0.6 m over
  the floor, so anything under about 0.7 m is seen from above.
- A phase name with a space cannot be passed to `reef:look` or `reef:find`: name phases with one word.
- `reef:find` and `reef:census` give no positions: when a species is "none within 84 m", try other reefs. The
  "refused by a species' own cap" count is not reset by a teleport.
- `cam=0,25,3` (the top-down camera) stops chunks being dealt: everything reads "none" from up there.
- `shotclean` leaves the diver's gauge panel and toasts in the shot; shots asked for less than about a second
  apart are sometimes dropped, and the file numbers do not follow the script's seconds: open them all.

**Runs and numbers**
- Wait for `build.sh` itself to finish (`Result: Succeeded` in its own output) before `shoot.sh`: a game started
  while the dylib is still linking says "Incompatible or missing module", hangs on a dialog and holds the game
  lock until you kill it.
- Frame times and `cpu` lines taken while the user's packaged game runs are noise (the dens read 8 to 60 ms a
  frame that way, 0.02 on a quiet machine): take them twice, and say so when the machine was busy.
