# reefreal: the audit (criterion 1)

Every animal and plant kind in the game, by system. "In the Keys" is yes / seasonal or rare / no. Sizes are what
the code holds: catchable fish as the weight range in pounds (`KeysData.cpp`) and the swimming model's length in
metres (`KeysLife.cpp` `Len`); reef animals as their `FPhase` lengths; birds as wingspan (drawn 1.5 times life
size on purpose, so they read from a boat: `KeysMangroveEcoBirds.cpp`).

How I know: no outside lookup was made for this audit. Every verdict is from my own knowledge of the species
(range, usual and record sizes, colour, Florida rules), checked against the names, numbers and texts in the
source files named. Where I am not sure, the row says "doubtful" and nothing was changed.

Fixes are numbered requests (R1-R4, in `.marshal/requests/reefreal/`) or, for the Abyssal Garden, done in the
lane's own files.

## 1. Catchable fish (`KeysData.cpp` `AllSpecies`, 56 rows; seen swimming through `KeysLife.cpp`)

| Key | Name shown | In the Keys | Size, colour, text | Verdict |
|---|---|---|---|---|
| bonefish | Bonefish | yes | 3-12 lb, 0.65 m, grey-silver; release-only text right | right |
| permit | Permit | yes | 8-40 lb, 0.75 m | right |
| tarpon | Tarpon | yes | 30-160 lb, 1.8 m, silver; harvest-tag text right | right |
| barracuda | Great Barracuda | yes | 5-45 lb, 1.3 m, silver with dark blotches | right; card length was short (R3) |
| snook | Snook | yes | 4-30 lb, 0.85 m, black lateral line | right |
| redfish | Redfish | yes (Florida Bay side, scarce on the ocean side) | 4-28 lb, 0.8 m, copper with a tail spot | right; the game already keeps it to bay flats, creeks and backcountry |
| trout | Spotted Seatrout | yes (Florida Bay) | 1-8 lb, 0.55 m | right |
| mangrove | Mangrove Snapper | yes | 1-12 lb, 0.42 m | right |
| yellowtail | Yellowtail Snapper | yes | 1-7 lb, 0.42 m, yellow line and tail | right |
| mutton | Mutton Snapper | yes | 4-22 lb, 0.65 m | right |
| hogfish | Hogfish | yes | 2-20 lb, 0.6 m, pink-orange | right |
| grouper | Black Grouper | yes | 10-80 lb, 1.15 m | right |
| mahi | Mahi-Mahi | yes | 6-55 lb, 1.25 m, green and gold | right |
| blackfin | Blackfin Tuna | yes | 6-40 lb, 0.8 m; "humps" text right | right |
| wahoo | Wahoo | yes | 15-90 lb, 1.6 m, barred | right |
| sailfish | Sailfish | yes (best in winter, as the text says) | 30-100 lb, 2.4 m | right |
| yellowfin | Yellowfin Tuna | seasonal or rare (more a Bahamas and Gulf fish) | 20-220 lb, 1.5 m | right; kept, weight 0.8 offshore |
| bluefin | Atlantic Bluefin Tuna | seasonal (spring, through the Straits, as its text says) | 150-900 lb, 2.3 m | right; kept |
| albacore | Albacore | rare | 15-60 lb, 1.0 m; text says "a rare visitor this far south" | doubtful: hooked rarely (0.3) but SEEN often (`LifeZoneLife` Offshore 1.2, `SpawnTunaNear` 2.5 against yellowfin's 5). Listed, not changed |
| cobia | Cobia | yes (mostly winter and spring) | 10-90 lb, 1.3 m | right |
| amberjack | Greater Amberjack | yes | 15-120 lb, 1.3 m; Marathon Hump text right | right |
| kingfish | King Mackerel | yes (winter) | 8-60 lb, 1.4 m | right |
| cero | Cero Mackerel | yes | 2-12 lb, 0.7 m | right |
| jackcrevalle | Jack Crevalle | yes | 4-35 lb, 0.85 m | right |
| ladyfish | Ladyfish | yes | 1-4 lb, 0.6 m | right |
| sheepshead | Sheepshead | yes, uncommon (a winter fish of bridges and pilings, commoner up the coast) | 2-12 lb, 0.55 m, barred | right; kept to Bridge and Hawk Channel |
| pompano | Florida Pompano | yes (winter) | 1-6 lb, 0.42 m | right |
| tripletail | Tripletail | yes | 4-30 lb, 0.65 m | right |
| lionfish | Lionfish | yes, as an invader (text says so) | 0.5-3 lb, 0.35 m, red-brown and white bars | right |
| goliath | Goliath Grouper | yes | 100-500 lb, 2.2 m; protection and lottery text right | right |
| graytrigger | Gray Triggerfish | yes | 2-10 lb, 0.45 m | right |
| blacktip | Blacktip Shark | yes | 30-150 lb, 1.7 m | right |
| bullshark | Bull Shark | yes | 100-350 lb, 2.3 m | right |
| lemonshark | Lemon Shark | yes | 60-250 lb, 2.3 m, yellow-brown; state-waters protection right | right |
| hammerhead | Great Hammerhead | yes | 150-700 lb, 3.3 m; release rule right | right |
| marlin | Blue Marlin | yes (Gulf Stream, summer) | 150-800 lb, 3.3 m; "over 300 lb are females" right | right |
| swordfish | Swordfish | yes (deep, off the reef) | 80-400 lb, 2.6 m; daytime-depth text right | right |
| blackmarlin | Black Marlin | **no**: an Indo-Pacific fish; a handful of strays have ever been recorded in the Atlantic | 200-900 lb, 3.6 m; its own text says "rarely seen this side of the Pacific" | **Left exactly as it is, at the user's word.** Not one field, weight or text of it is touched by any request here (its share of offshore bites moves with every other fish's when the halibut goes: 2.1% to 2.2%) |
| gag | Gag Grouper | yes | 6-45 lb, 0.9 m; "big ones are males" right | right |
| redgrouper | Red Grouper | yes | 5-30 lb, 0.75 m; hole-digging text right | right |
| flounder | Southern Flounder | **no**: its range ends north of the Keys on both coasts | 1-12 lb, 0.5 m, one black tail spot (the redfish's pattern); painted back-to-belly like an upright fish, so its upper flank faded from brown to white | **wrong: R2** makes it the gulf flounder (0.5-6 lb, 0.38 m, three eye-spots), same key, and paints flatfish by their sides (eyed side sand-brown, blind side white) |
| sole | Hogchoker Sole | rare (a fish of mainland rivers and estuaries; the lined sole is the Keys' own) | 0.4-4 lb, 0.26 m; text calls it "a true sole" | species doubtful, kept. **Size and text plainly wrong: R4** (a hogchoker is an American sole, family Achiridae, and reaches about 8 in and a few ounces, never 4 lb) |
| halibut | Atlantic Halibut | **no**: a cold-water fish of the North Atlantic, south to about Virginia | 3-400 lb, 1.25 m; hooked in Hawk Channel and offshore, seen offshore | **wrong: R1** retires it |
| mako | Shortfin Mako | seasonal (winter, offshore) | 80-600 lb, 3.6 m (a very large one) | right |
| warsaw | Warsaw Grouper | yes (deep ledges and wrecks) | 60-440 lb, 2.3 m (as long as they get) | right; text "live past fifty years" is doubtful (41 is the oldest I know of), not changed |
| cubera | Cubera Snapper | yes | 25-120 lb, 1.6 m (as long as they get); summer spawning text right | right |
| tigershark | Tiger Shark | yes | 200-900 lb, 4.2 m | right |
| bluerunner | Blue Runner | yes | 1-8 lb, 0.5 m | right |
| schoolmaster | Schoolmaster Snapper | yes | 1-8 lb, 0.45 m, yellow fins | right |
| lanesnapper | Lane Snapper | yes | 0.5-6 lb, 0.34 m | right |
| vermilion | Vermilion Snapper | yes (deeper reef) | 1-7 lb, 0.4 m | right |
| lookdown | Lookdown | yes | 0.5-4 lb, 0.3 m | right |
| palometa | Palometa | yes | 0.5-3 lb, 0.3 m, dark bars and streamers | right |
| spanishmack | Spanish Mackerel | yes (winter) | 1-9 lb, 0.6 m; text says a Spanish has "no bars" | **text wrong: R4** (a cero is told by the stripe along its side, not by bars) |
| bigeye | Bigeye Tuna | rare (deep, far offshore) | 30-300 lb, 1.6 m | doubtful, kept (weight 0.7) |
| skipjack | Skipjack Tuna | yes | 4-30 lb, 0.75 m | right |

Length for weight (criterion 5): the catch card printed `KeysMesh::FishLength x 1.1`, one formula for every fish:
inches = 11.7 x the cube root of the pounds. That is the build of a bass, grouper or tuna. Checked against a
typical fish of each of the 55 species still in the game: right within about 10% for 23 (the groupers, tunas,
snappers, jacks, permit, pompano, goliath, warsaw), 10-45% short for the long fish (barracuda 32 in at 20 lb
against about 45 in life, tarpon, snook, wahoo, the mackerels, mahi, cobia, ladyfish, every shark) and about
half length for the billfish and the swordfish (sailfish 43 in at 50 lb against about 84), slightly long for the
deep-bodied ones (sheepshead, triggerfish, tripletail). **R3** gives each species its own figure.

## 2. Seen but not catchable (`KeysLife.cpp` `LifeVisTable`, `LifeZoneLife`, the Humps table)

| Key | Name shown | In the Keys | Size, colour | Verdict |
|---|---|---|---|---|
| parrotfish, angelfish | none (generic) | yes | 0.55 m, 0.38 m | already out of every zone table (earlier lanes); only the rows remain |
| flyingfish | none | yes (offshore) | 0.32 m, blue | right |
| nurse | none | yes | 2.2 m, brown | right |
| stingray | none | yes (southern stingray) | 1.0 m | right |
| eagleray | none | yes (spotted eagle ray) | 1.1 m, dark with white dots | right |
| turtle | none | yes | 1.0 m | right |
| manatee | none | yes | 3.0 m, grey | right |
| dolphin | none | yes (bottlenose) | 2.4 m | right |
| pilotwhale | none | yes (short-finned, offshore) | 6 m, black | right |
| humpback | none | rare (a few pass in winter on the way to the Caribbean) | 15 m | doubtful, kept: 0.02 in the offshore table, about 1 offshore sighting in 2,000 |
| spermwhale | none | rare (lives in the Gulf and the Straits in a thousand metres and more) | 14 m | doubtful, kept: 0.3 in the Humps table, about 1 hump spawn in 80. The humps are shallow for it |
| whaleshark | none | rare (summer, mostly the Gulf side) | 9 m, spotted | doubtful, kept (0.04) |
| giantmanta | none | rare (commoner up Florida's east coast) | 2.4 m | doubtful, kept (0.04); see the Abyssal Garden's three |
| greatwhite | none | rare (winter; tagged ones do round the Keys) | 5 m | doubtful, kept (0.025) |
| mola | none | rare (Gulf Stream) | 3 m (a very large one; 1.8 m is usual) | doubtful, kept (0.03) |
| sawfish | none | rare, and this is the place for it: the Keys and Florida Bay are the smalltooth sawfish's last United States stronghold | 4.2 m | right; never catchable, which is right for an endangered fish. Drawn with the shark body: whether it has a saw was not checked |
| silverside, herring, grunt | none (school fish) | yes | 0.11, 0.2, 0.3 m | right |
| goldbluefin | none | not a real animal: the game's own legend (a gold bluefin placed by the clock, never caught) | 3.1 m | not a realism matter of the Keys; listed, not changed |

## 3. Schools (`KeysSchools.cpp`)

silverside, herring, schoolmaster, grunt, lane snapper, yellowtail, black grouper, red grouper, barracuda,
bonefish, permit, blue runner, jack crevalle, blacktip, bull shark: all Keys fish, lengths 0.09-2.4 m in range
for each. Nothing to change.

## 4. Reef life (`KeysReefLife*.cpp`; each named in its first-sight toast with its scientific name)

| Key | Name shown | In the Keys | Size | Verdict |
|---|---|---|---|---|
| stoplightparrot | Stoplight parrotfish (Sparisoma viride) | yes | two phases | right; "protected in Florida" is right (no take of parrotfish for food) |
| rainbow parrotfish | Rainbow parrotfish (Scarus guacamaia) | yes, uncommon | 0.5-0.9 m | right ("the biggest parrotfish in the Atlantic" is right) |
| midnight parrotfish | Midnight parrotfish (Scarus coelestinus) | yes | 0.5-0.75 m | right |
| bluetang | Blue tang (Acanthurus coeruleus) | yes | 0.15-0.25 m, juvenile 0.05-0.10 | right |
| queen, French, gray angelfish | Holacanthus ciliaris, Pomacanthus paru, P. arcuatus | yes | 0.30-0.45, 0.30-0.40, 0.35-0.50 m | right |
| sergeant major (and the dark male on eggs) | Abudefduf saxatilis | yes | 0.12-0.18 m | right |
| queen conch | Aliger gigas | yes | shell 0.20-0.30 m | right; protected in Florida, as the toast says |
| giant Caribbean anemone | Condylactis gigantea | yes | 0.15-0.30 m | right |
| southern stingray | Hypanus americanus | yes | disc 0.45-1.5 m | right |
| green turtle | Chelonia mydas | yes | shell 0.8-1.1 m | right |
| loggerhead | Caretta caretta | yes | shell 0.8-1.0 m | right |

## 5. Corals, sponges and plants of the reef (`KeysReef.cpp`; none is named to the player)

elkhorn, staghorn, pillar, brain, star and finger coral, sea fan, sea rod, lettuce coral, black coral (25 m and
deeper), barrel sponge, azure vase sponge, elephant ear sponge, algae, attached sargassum, turtle grass: every
one is a Florida Keys kind and the depth bands are the real ones. Their looks were audited by `reefbottom`
(its status file); nothing here to add.

## 6. Mangroves (`KeysMangroveEco*.cpp`, `KeysMangroveLife.cpp`, `KeysMangrove.cpp`; named only in logs)

| Kind | In the Keys | Size, colour | Verdict |
|---|---|---|---|
| great egret | yes | span 1.4 m, white, yellow bill, black legs | right |
| great blue heron | yes | span 1.8 m | right |
| little blue heron | yes | span 1.0 m, slate | right |
| white ibis | yes | span 0.95 m, white, red bill and legs, black tips | right |
| roseate spoonbill | yes (Florida Bay) | span 1.25 m, pink | right |
| cormorant (double-crested) | yes | span 1.2 m, black | right |
| anhinga | yes, uncommon in salt water (a fresh-water bird, commoner on the mainland) | span 1.15 m | doubtful, kept |
| belted kingfisher | seasonal (winter) | span 0.5 m, blue-grey and white | right |
| green heron | yes | span 0.65 m | right |
| night heron (yellow-crowned by its grey body and dark head) | yes | span 1.1 m | right |
| tern | yes | span 0.8 m, white, black cap, orange bill | right |
| osprey | yes | span 1.6 m | right |
| brown pelican | yes | span 2.1 m | right |
| silverside, pilchard, juvenile snapper and grunt, needlefish, mullet | yes | small | right |
| barracuda, lemon shark (small), nurse shark, stingray, snook, jack, juvenile tarpon | yes | | right |
| upside-down jellyfish (Cassiopea) | yes | | right |
| mangrove tree crab, fiddler crab, horseshoe crab | yes | | right |
| manatee | yes | | right |
| red mangrove | yes | | right |

## 7. Other birds and animals

| Kind | Where | In the Keys | Verdict |
|---|---|---|---|
| gull, tern, frigatebird, brown pelican | `KeysLife.cpp` (the birds over bait), spans 1.25, -, 2.1, 2.1 m | yes (the magnificent frigatebird is a Keys bird) | right |
| green iguana | `KeysTownFame.cpp` | yes, as an invader | right |
| dog | `KeysDog.cpp` | a pet | not a wildlife matter |
| koi | the Japanese-garden estate's pond | a pond fish | not a wildlife matter |

## 8. The Abyssal Garden (`KeysGarden.cpp`, this lane's own)

| Kind | In the Keys | Verdict |
|---|---|---|
| kelp forest (about 170 stalks, 8-15 m tall) | **no**: kelp is a cold-water plant; nothing like it grows in tropical water | **replaced** by 270 colonies of sea plumes, sea rods, sea whips, wire coral, black coral bushes, deep-water sea fans and rope sponges on 36 rock ledges, with stove-pipe, yellow tube, brown tube, elephant ear and more giant barrel sponges |
| barrel sponges in purple, orange and tan | the sponge yes, those colours no | **recoloured** red-brown to grey, as a giant barrel sponge is |
| sea fans, purple and yellow | yes (the common sea fan comes in both) | right |
| three manta rays lapping the site, 4.8-6.4 m across | rare | doubtful, kept: a giant manta is real here but three in permanent residence over one site is the scene's licence, and it is the site's drama |
| magenta glowing ground cover | no such thing glows by day | doubtful, kept: scenery in the style of the scene it is built after; listed for the user to decide |
| stone whale fluke, wooden ship, rock arch | not living things | - |
| the name "Abyssal" for a site 11-16 m down | - | a name; not changed |

## 9. Seen while auditing, outside animals and plants

- An "Oil Rig" fishing zone and rig markers exist (`KeysLife.cpp`): there are no oil rigs off the Florida Keys.
  Not an animal or plant, so not in this lane's goal; listed for the marshal.
- A passing boat is named "JUST FOR THE HALIBUT" (`KeysTownKit.cpp`): a real-world pun boat name, left alone.
