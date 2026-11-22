# R2: sound for the mangrove ecosystem (hub: `KeysAudio.*`, `KeysSfxSynth.*`)

Nothing here is done and nothing was bought. The lane's side is ready: the ecosystem queues what made a noise, and
the audio hub takes it with one call.

## The hook (already in the lane's files)

```cpp
#include "KeysMangroveEco.h"
// in AKeysAudio::Tick:
FName What; int32 Kind; FVector Where; float Power;
while (KeysMangroveEco::PopSound(What, Kind, Where, Power)) { /* play it at Where (game metres), gain by Power */ }
```

- `What` is one of the names below, `Kind` is the animal (`KeysMangroveEco::EKind`, name from `KindName(Kind)`).
- Only events within 250 m of the camera are queued; at most 16 wait. If nobody polls, the queue just drops the old ones.
- `PopSound` returns false on the classic map and with `-KRNoMangroveEco`.

## Events, and what would sell each

| `What` | When it fires | Sound wanted | Could be made today with |
|---|---|---|---|
| `shower` | predators rush a school and the bait leaves the water | a hiss of many tiny splashes, 0.6 s, then patter | `AKeysAudio::Splash(x, z, 30, 0.5)` twice, 80 ms apart |
| `plunge` | a tern, pelican, osprey or kingfisher hits the water | pelican: a heavy slap; tern and kingfisher: a small plip | `Splash` with the event's `Power` |
| `jump` | a mullet lands | one flat slap | `Splash(x, z, 6, 0.5)` |
| `roll` | a tarpon rolls | a soft gulp and swirl | `Splash(x, z, 3, 0.3)`; a real gulp needs a sample |
| `crabdrop` | tree crabs let go of a root | two or three light plops | `Splash(x, z, 2, 0.2)` |
| `strike` | a wader's bill hits the water | a short wet tick | `Splash(x, z, 2, 0.25)` |
| `flush` | a bird is put up by the boat | wingbeats (three to five soft whumps, slower for the big herons) and an alarm call | needs a sample or a new synth voice |
| `croak` | a heron or egret strikes or is flushed | great blue: a harsh "fraaank"; egret: a dry croak | new synth voice (a rough saw, falling) or a sample |
| `honk` | white ibis feeding | a nasal grunt, "hunk hunk" | sample |
| `rattle` | a kingfisher leaves its root to dive | the dry machine-gun rattle | new synth voice (fast noise bursts) or a sample |
| `kree` | a tern turns for a shower of bait | a sharp "kee-arr" | the existing gull voice pitched up would pass |
| `whistle` | an osprey starts to hover | a run of high chirps, "cheep cheep cheep" | new synth voice (short sine chirps, rising) |
| `quok` | reserved: a night heron leaving at dusk | one flat "quok" | sample |
| `flight` | a rookery flight leaves or comes in | many wings, a wash of calls | sample bed |

Also wanted, not tied to an event (the hub would drive them from `kr.eco stats` style counts, which the lane can
expose as a function if asked):

- a rookery bed within 300 m of a rookery key: grunts, bill clatter, squabbling; louder at dawn and dusk
- a mangrove edge bed at idle speed: water lapping in the roots, clicks of snapping shrimp, the odd mullet
- at night: the insects up, the day birds silent

## What from the paid sound library (needs the user's OK through the marshal; nothing bought)

In order of how much each would add:

1. Wading bird calls: great blue heron, great egret, white ibis, night heron (4 short one-shots each).
2. Wingbeats of a large bird taking off (3 takes), and a small flock taking off (2 takes).
3. Belted kingfisher rattle (2), osprey call (2), royal or least tern call (3).
4. A heron rookery ambience loop, 30-60 s.
5. Mangrove shoreline water lapping loop, and snapping shrimp, 30-60 s each.
6. A tarpon roll or gulp (2), a mullet jump (3), a bait shower (2).

All are recordings of animals and water: no brands, voices or music. Until then the splash events above can use the
project's own synth splash, and the gull voice can stand in for the tern.
