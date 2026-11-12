# Keys Run (Unreal Engine 5)

An Unreal Engine 5 port of the three.js game in `~/Downloads/keys-run-deploy.zip`: center console fishing in Marathon, Florida Keys.

Nearly the whole game is ported. Everything is built in C++ at runtime, the same way the JS game builds it in code,
so there are no hand-made assets. The editor script creates the materials and the empty map the first time you open the project.

## What's in it

- **The map:** every island, creek, channel, hump, reef line and marina from the JS game, using the same `landH` / `depthAt` /
  `zoneAt` math (`KeysGeo.cpp`), plus Miami and Biscayne Bay. The keys and the Miami islands have their real OpenStreetMap
  coastlines (the web game's `world/coast-data.ts` and `coast.ts`: bays, spits, canals and points, with the same edge-grid
  signed distance), US-1's causeway fill, the town pad, the golf courses and the runway hold the ground up, and the marina
  basins are dredged, so the marinas, the town, the fishing piers, the estates and the race marks all sit at the web game's
  own coordinates. US-1 and the Seven Mile Bridge
  (barrier walls, striping, bents with pier caps, the hump over the channel) and the old bridge (lower and narrower, arch
  ribs, the missing swing span and its broken stubs) have pilings you can hit; US-1 runs across the keys as a road. The keys
  have real palms, mangroves and conch houses (each its own size, with a hip tin roof, a porch, shuttered windows, some up
  on piers), 31 waterfront estates with pools, private docks and a boat on a lift (walked along the real shoreline as
  `world/waterfront.ts` does), and the web game's six public fishing piers at its own spots. Plus the Miami skyline.
- **Boot Key Harbor City (the town):** the town built in the web game, at Boot Key Harbor City Marina, laid out from the same
  tables (`world/town.ts`, `world/casino.ts`, `content/venues.ts`): the fill pad with its seawall, rails and promenade;
  Harbor Drive, Palm Avenue, the back lane and Sombrero Boulevard with sidewalks, kerbs, markings and crossings; sixteen
  buildings in their own paint with their names and taglines on the sign bands, windows, awnings and roof signs (the
  tackle shop's marlin, the Pink Flamingo's flamingo, the gun shop's target, the Rusty Anchor's beer mug, the diner, the
  ice-cream cone, the casino's gold marlin); parked cars and boat trailers, palms, lamps, benches, the marlin statue and
  the motel's pole sign; people standing about and walking the sidewalks. Every building is a walk-in room with its
  counter, shelves of stock, booths, racks and staff: the Salty Dog Diner, the Rusty Anchor, the Tipsy Tarpon and Reel
  Hammered, Hook Line & Sinker, Deadeye Guns, Cheap Beer & Lotto, Conch Republic Gifts, Keys Pawn, the coin laundry,
  Scuba Steve's, the Sunset Motor Lodge, Cone Zone and the Stop-N-Go; the Pink Flamingo Cabaret with its stage, two
  dancers, the bar and the DJ; and the Gaff & Gill Casino with its slot banks, the cage, the Go Fish table, the marlin
  fountain and the aquarium wall. Harbor Row's four lots behind the casino are for sale.
- **Ashore (Q):** the houses, palms, mangroves and estate houses out on the keys are solid, and the dark rooms (the casino,
  the Pink Flamingo, the bars) hold their exposure so they stay moody instead of washing out. Tie up alongside any dock (the four marinas, the fishing piers, an estate's dock) when you are slow,
  and walk the docks, the town and the keys. F at a counter opens its shelf: buy with the cash the fish house pays for
  your fillets at the weigh-in (game fish, billfish and protected species don't sell). The heavy rod holds more tension,
  the lever reel brings fish in faster, diner coffee widens the strike window, live bait brings quicker bites, the dive
  shop's pony bottle, wetsuit and fins stretch your breath hold, the motel sleeps you through to sunset, the slots and
  scratch-offs pay out (sometimes), the gift shop dresses you, and a Harbor Row lot gets you a villa. Food and drink
  are timed buffs with their own strip on screen and a toast when they wear off (web `game/buffs.ts`, `sim/shop.ts`,
  `sim/steady.ts`): coffee, the mahi sandwich, key lime pie, wings, ice cream and snacks steady your hands (a longer strike
  window, less wobble in the first-person view), the fisherman's breakfast also holds the line a little harder in a fight,
  the mahi sandwich stretches a breath hold, and a beer, a key lime shot or a six-pack does the opposite: a shorter strike
  window, a view that sways, and too many to dive. Cash, gear and
  property are saved with the leaderboard.
- **Landmarks:** Sombrero Key Light on the reef crest (the skeletal iron pyramid with the keeper's house and lantern),
  Faro Blanco's lighthouse, the reef's mooring-ball line, and numbered red nuns and green cans down both sides of every
  dredged channel and marina entrance (red right returning), riding the swell. The marinas have the web game's docks:
  main pier, T-head, finger piers, pilings, boats in the slips, the harbourmaster's sheds and dry-stack racks.
- **The sea:** the same 8-wave swell and chop, and smooth Kelvin wakes (V-shaped, at the real 19.5° angle) from every boat.
  The water is see-through: `M_KR_Ocean` is Unreal's Single Layer Water, so you look down through real refraction and per-channel
  absorption at the sea floor. The flats are clear turquoise over sand and seagrass, Hawk Channel is green-blue, and the Gulf Stream is
  deep blue. The material also adds wind ripples that catch the sun, light glowing through thin wave tops, and foam broken into
  real-looking patches on breaking crests, along fresh wake arms and at the beach.
- **The sea floor:** real bathymetry plus the web game's relief (rock outcrops, coral heads, spur-and-groove on the reef front, the
  terraced reef wall, sand waves), colored sand, seagrass, rubble, coral and deep rock by depth and zone, with moving caustics.
  It streams in 64 m chunks around the camera with more detail up close (finer still under water), plus a coarse skirt to the horizon.
- **The reef:** elkhorn, staghorn, brain and star coral, sea fans and plumes, barrel and tube sponges, encrusting mounds and seagrass
  beds, placed deterministically on Sombrero Reef, the patch reefs and the Hawk Channel micro-patches (seagrass on the flats), at three
  levels of detail. Fans, plumes and grass sway with the current and the wave surge.
- **Diving and spearfishing (J):** jump over the side and free-dive. Breath-hold air that burns faster with depth and effort,
  buoyancy that lifts you toward the surface, fin kicks in pulses, drag along and across the body, the current pushing you around,
  narcosis below 25 m, blackout and shallow-water blackout (you wake up on the boat), and an ascent warning that gets louder as the air
  runs out. Dive-mask and chase views (1). Under water there is per-channel light extinction, depth-banded visibility, Snell's window
  overhead, caustics, marine snow, your own bubbles and seep bubbles on the bottom, and the lens wetting when you cross the surface.
  The speargun has a hauled reload (R), a drag-limited shaft with aim assist that leads moving fish, and a float-line fight (hold S to
  haul, ease off before the shaft tears free); a landed fish goes on the same catch card, cooler and score as a rod catch.
  Fish stand off a diver who is finning hard, bolt from a near-miss, and fill in around you in schools on the reef.
  Planing boats leave a white prop-wash trail that spreads, breaks up and fades to pale green churned water. H cycles the four sea states.
- **Boats:** all eight (Robalo R230, Grady-White Canyon 306, Freeman 42LR, Midnight Express 43, MTI V42, Yellowfin 36 CC,
  Contender 32ST and Nor-Tech 390 Sport) with their real specs (length, beam, engines, top speed, acceleration, turning, draft,
  cooler). Each hull is the web game's loft (`entities/boat/hull.ts`, `KeysHull.cpp`) from its own `HULLS` row: deadrise that
  warps from the transom to a deep-V forefoot, chine flats, flared and bulged topsides, sheer spring, a raked stem, the boot
  stripe, cove stripe and rub rail in the boat's paint, and sharp notches for the steps; the Freeman is a catamaran with slim
  asymmetric sponsons under a bridge deck. The fit-out comes from the web game's `content/boats.ts` hull styles: T-top or hardtop, paint, boot and cove stripes,
  leaning post with a rocket launcher of rods or helm seats, rod racks on the top, radar, bow rail, bow sun pad on the go-fast
  boats, steps in the running bottom, engines on the transom or set back on a bracket, cowl and lower-unit colours. The
  Yellowfin is a cream hull with navy trim and triple outboards and the fine 24-degree entry; the Contender is white and green;
  the Nor-Tech is black and orange with a stepped bottom. Boats are bought from the broker with the cash from your fillets
  (`sim/economy.ts`): the five original boats (Robalo, Grady-White, Freeman, Midnight Express, MTI) are always yours, so a save
  from before the broker keeps them all; the Contender is $20,000, the Yellowfin $30,000 and the Nor-Tech $85,000. In the main menu, Left / Right on the Boat row shows each
  one and Enter buys it. The physics is ported line for line: 15-point buoyancy, trim and hole shot, prop ventilation, wave
  surfing, slams and air, beaching, reverse and neutral.
- **Outriggers and trolling (=):** the tower boats (Grady-White and Freeman) carry two long outriggers on swivel bases that swing
  out from stowed upright to about 70 degrees and rake aft (web `entities/boat/model.ts`); the Yellowfin and Contender have fixed
  rigger poles on the T-top. = puts the spread out and the boat holds trolling speed by itself (about 5 kn on the gauge; the lures
  swim from 4 to 6.5 kn, best in the middle, and the panel shows where you are in the band). The spread is the web game's
  (`game/fishing/troll-spread.ts`): ballyhoo under pink skirts on the two long rigger lines, skirted lures on two flat lines down the
  prop wash and a shotgun bait 96 m back down the middle, plus a pair of squid daisy-chain teasers splashing close astern. The lines
  run from the rods up to the rigger clips and back to the baits, trail along the boat's own track through a turn, and the lures skip,
  weave and roll on their own beats. Strikes follow `sim/troll.ts`: the strike clock runs in proportion to how well the lures are
  swimming and twice as fast over a weedline, a bird pile or a rig; tuna, billfish and wahoo are what a spread is set for; a good
  spread skews the fish heavy. The mate calls "Port rigger - FISH UP!", the camera swings round over the console to watch the fish
  come up behind the bait (`troll-raise.ts`), it eats, the bait goes under, and it is the rod's own fight from the transom (the
  boat drops out of gear; jumpers come out of the water), the same catch card, cooler and score. After a fish the lines are cleared
  for a few seconds before the boat goes back on the troll. = again brings the lines in.
- **The Marathon race (R):** with the lines in, R starts the web game's race (`sim/race.ts`, `game/race/**`): two laps of nine
  marks (Start / Finish, Hawk 1, Hawk 2, East Turn, Reef East, Sombrero, Reef West, West Turn, Inshore), about 6.4 km a lap, against
  Reel Deal (an MTI), Sand Bar (a Midnight Express) and Conch Runner (a Freeman), who run their own lines round the course, back off
  through the turns and throw wakes. Orange race marks with a numbered flag (the start mark is yellow), a green pennant and a ring of
  floats over the mark you need next, the course and the field on the chart, and a board with your place, lap, clock, the next mark
  and its distance, and the running order. Marks within 55 m count as rounded. R again abandons it; your best time is saved.
  The marks are at `sim/race.ts`'s own coordinates.
- **Fishing tournament (Esc menu):** the web game's tournament (`sim/tournament.ts`, `game/tournament/**`): five species drawn
  for the day from the ones that are common somewhere and legal to keep, a 15-second countdown, fifteen minutes to keep one of
  each, then run them in to Boot Key Harbor and slow down inside it. The three AI boats' days are planned from the tournament's
  seed (their catches come in on the toasts) and the first boat home wins; if nobody makes it, the horn ends it. A board shows
  the five with ticks, the time left, the distance home once you have all five, and the standings; wins go in your trophy case.
- **Fishing:** charge-and-release casting, nibbles and strikes, the fight (tension, drag, stamina, runs, jumps, tail-walks, sounding),
  auto-chase, all 40 species with the same zone tables, offshore size scaling, both grand slams, the catch card, the cooler and capacity,
  catch-and-release rules, and marina weigh-ins.
- **Menus:** a main menu (start, boat, play online, leaderboard, settings, controls, quit) and an Esc pause menu that stops the world,
  usable with the mouse or the keyboard. Settings covers sea state, time of day, sound, radio, camera, fishing view and auto-chase.
- **Fishing presentation:** the catch is held up side-on to the camera and kicks; fish models are smooth-shaded; when a hooked fish
  jumps the camera zooms in and follows it, with a crown of white spray on the way out and back in; hi-vis yellow line.
- **HUD:** helm gauges, nautical chart (M enlarges it), cast, strike and fight meters, and toasts.
- **Views:** the chase camera, a first-person view from the helm (1), and a first-person fishing view with your own rod, reel and hands
  that comes on whenever you cast (V turns it off). It follows the float or the hooked fish unless you look around with the mouse.
- **Electronics:** G shows the glass-helm display: a chartplotter with depth bands, contours, your track, marinas, SOG / COG / depth and
  lat-long, and a 200 kHz fish finder with the bottom return, depth scale and fish arches. [ and ] change the chart range.
- **Leaderboard:** B opens it. It keeps your biggest fish, your top catches, the record for each species and your best score,
  saved on this computer (`Saved/SaveGames/KeysRunBoard.sav`). The JS game shared its board with friends online; this one is local.
- **Sound:** everything is synthesized live like the JS game's WebAudio, with no sound files. That covers the engine (cylinders, lope, intake and supercharger whine, starting up with I),
  water rush, wind, hull slap, the reel's drag and ratchet, and splashes, strikes, slams and line snaps. There are three procedural radio stations
  (Beno Bonanza, Margaritaville, Hip-hop) with DJ lines. P changes the station and N mutes everything.
- **Traffic:** 23 AI boats (center consoles, sportfishers, sailboats, wake boats with a surfer, jet skis) running 14 routes, plus 3 racers,
  all riding the waves and throwing wakes. Bump one and you'll hear about it. They show on the chart.
- **The living sea:** pelican flocks, gulls, terns and frigatebirds; 40 bird piles that cycle into feeding frenzies; sargassum weedlines;
  four oil rigs you can fish (and hit); and visible schools of all the species with tuna that run down a bait left soaking offshore.
  Casting into a school bites faster and bigger, and rigs and weedlines have their own fish tables. All of it is marked on the chart.
- **Extras:** the buddy boat (Y), the shower (Z), travel to Miami and back (X), the crew, party and flags on deck, Caelen the parrot,
  and Luigi mode: trim all the way up at speed and see what happens.
- **Sky:** physically based sky, clouds, fog, and O for sunset over the Seven Mile Bridge. At sunset Sombrero Key Light's lens
  turns (`world/lighthouse.ts`: a bright narrow beam and a wide faint one, front and back, every 9 s, 620 m out) and the town
  glows: halos round the street lamps with pools of light under them, and the neon along every sign (`entities/town/kit.ts`).
- **Shipwrecks:** the web game's six (`shared/world/wrecks.ts`): a DC-3, a sand barge, a harbor tug and three freighters, placed
  by the same hashed search against the real depth, zone and shore, lying along the contour with a list and the broken ones
  in two pieces. You can see them through the clear water and dive on them; they are on the chart (a hull line with a cross),
  and a cast within 28 m fishes the wreck table (amberjack, goliath and gag grouper, cobia, barracuda, jacks, sharks).
- **Visitors:** the pirates (`entities/life/pirates.ts`, `shared/world/pirates.ts`): Cap'n Barnacle Bev and Mr. Mildew in a tired
  wooden gaff sloop sail up once every half hour on the wall clock, heave to 44 m off, hail you, appraise your cooler and your
  wallet in the web game's own lines, chat, and sail off; they never take anything. And a banner plane
  (`entities/life/banner-plane.ts`) drones past every few minutes towing one of the town's businesses' slogans.

- **Online with friends:** press L, type a name and a crew code, and Enter. Everyone who uses the same crew code sees each other's
  boats live (with name tags, wakes you can ride, and green dots on the chart), gets "Sam landed a 41 lb cobia!" messages, and shares a
  Crew tab on the leaderboard (B). The game remembers your login and reconnects next time; L again logs out. See "Online play" below.

Town and land differences from the web game: facades and signs are geometry and text instead of painted canvases (no
window notices; the sunset glow is soft halos rather than bloom); the dancers are the game's own figures (the web game's rigged glTF dancers are not
ported); street life is people on loops along the sidewalks (no visit scripts, dogs, squirrels, gulls or litter);
estates other than Harbor Row and the estate builder are not ported; the casino's slots are a single shelf item rather
than the machine screen; and the shop shelves are a subset of the web game's (no handgun, flags or tackle-ladder rungs).

Not ported: the boat pistol (`sim/handgun.ts`), the commercial fleet, Larry's raft, the sandbar party, the boat dog and the
street animals, the wreck treasure hunt, the estate plat beyond Harbor Row, the tournament ceremony, the boats' glass-helm screens, cabin, deck lights and quilted upholstery details, the 3D screens on the dash (the electronics are a HUD panel instead),
spray particles (splashes are simple shapes), text-to-speech (the DJ and Luigi callouts are on-screen text),
engine sounds for other boats, and playing your own music files on the radio.

Diving differs from the web game in a few places: the dive shop's pony bottle, wetsuit and fins only lengthen the breath
hold (the wide mask and the gun shop's long gun are for show), and the speargun is the stock gun with unlimited shafts; the god rays (the web game's high/ultra quality
setting), the breathing loop in the underwater sound and the wreck treasure are not ported; the near-field reef schools are
ported on top of this port's existing fish population (the web game's resident and roaming layers are not); the diver
dives with J, so the shower moved to Z; and the underwater fog is a post-process rather than a per-material fog.

## Setup (Mac)

1. Install **Xcode** from the App Store, open it once, and accept the license.
2. Install the **Epic Games Launcher**, sign in, and go to Unreal Engine → Library → install **UE 5.6** (or newer).
3. Double-click `KeysRun.uproject`. When it says the KeysRun module is missing, click **Yes** to build it.
   - If it asks which engine version to use, pick the one you installed.
   - If the build fails, right-click the `.uproject` → *Generate Xcode project*, open `KeysRun.xcworkspace`, and build the `KeysRunEditor` scheme to see the errors.
4. The first time the editor opens, the Output Log shows `Keys Run: created ...` for the materials and the map `/Game/KeysRun/Maps/Keys`.
   Then open that map (it should already be open) and press **Play**.

To ship a standalone app: Platforms → Mac → Package Project.

## Controls

| Key | Action |
| --- | --- |
| A / D, Enter | choose a boat (Enter on the Boat row buys it), start |
| W / S | throttle / reverse (S also reels) |
| A / D | steer |
| U | gear D → N → R |
| T | trim mode (Up/Down trims); during a fight, T / Shift+T sets drag |
| Space (hold) | power a cast |
| Space | strike on the bite; hold to reel during a fight; release a landed fish |
| R | reel in |
| K | keep the fish |
| = | trolling: outriggers and lines out, hold trolling speed / lines in |
| R (lines in) | Marathon race: start / abandon |
| Esc, Fishing tournament | start / quit a fishing tournament |
| E | weigh in at a marina dock |
| F | auto-chase on/off |
| H | sea state |
| O | sunset |
| M | big chart |
| 1 | third person / first person from the helm |
| V | first-person fishing view on / off |
| G | chartplotter and fish finder |
| [ / ] | chart range in / out |
| B | leaderboard (Tab or A / D switch tabs, B or Esc closes) |
| I | motors on / off |
| P | radio station / off |
| L | log in to play online with friends / log out |
| Esc | pause menu |
| N | sound on / off |
| Y | buddy boat |
| Z | shower |
| J | jump in to dive / climb back aboard |
| Q | tie up at a dock and go ashore / back aboard (twice when far: a cab back to the dock) |
| **Ashore:** W A S D + mouse | walk where you look (Shift runs, arrow keys turn) |
| F | use a counter, a slot machine, a sign or a plaque |
| Up / Down + Enter, or 1-9 | pick and buy at a counter (F or Esc walks away) |
| 1 | third person / first person |
| X | travel to Miami and back |
| C / mouse wheel | zoom |
| **Diving:** W A S D + mouse | swim where you look |
| Space / C (or Ctrl) | ascend / descend |
| Shift | sprint |
| Click | fire the speargun |
| R | haul the bands back (reload) |
| hold S | haul a speared fish in |
| 1 | dive mask / chase view |
| Mouse drag | orbit the camera, or look around in first person |

## Code map

| File | What it is |
| --- | --- |
| `KeysGeo.*` | islands, depth, zones, waves, wakes, bridge pilings (ported from the JS) |
| `KeysData.*` | species, zone tables, boats |
| `KeysMesh.*` | the boats' fit-out, the outriggers, fish and tubes, soft glow blobs and fans |
| `KeysHull.cpp` | the web game's hull loft and each boat's `HULLS` row; deck shape for the fit-out and the crew |
| `KeysCoastData.h` | the islands' real outlines, generated from the web game's `coast-data.ts` (OpenStreetMap, ODbL) |
| `KeysWrecks.*` | shipwreck placement, hulls and the wreck fish table |
| `KeysVisitors.*` | the pirates' sloop and their visits, the banner plane |
| `KeysEvents.*` | the Marathon race (course, AI field, marks, standings) and the fishing tournament (species draw, AI plans, delivery) |
| `KeysBuffs.cpp` | timed food and drink buffs and what they do to your hands |
| `KeysTroll.cpp` | trolling: the speed window and strike clock, the raise and the hookup, the spread of lines, lures and teasers |
| `KeysWorld.*` | terrain, water surface, bridges, scenery, marinas, sky and sun |
| `KeysBoatPawn.*` | boat physics, camera, input, fishing loop |
| `KeysHUD.*` | all on-screen UI, including the chartplotter, fish finder and leaderboard |
| `KeysSave.*` | the local leaderboard save game |
| `KeysTraffic.*` | AI boat traffic and racers |
| `KeysLife.*` | birds, bird piles, weedlines, oil rigs and fish schools, and how they change the fishing; the near-field schools, flight and spear targets under water |
| `KeysSea.*` | sea floor relief and colors, currents and surge, deterministic hashing (ported from the web game) |
| `KeysSeafloor.*` | the streamed, LOD'd sea floor mesh |
| `KeysReef.*` | reef placement and the instanced coral, sponges, fans and seagrass |
| `KeysDive.*` | free-dive physiology and the speargun rules (pure) |
| `KeysDiver.*` | diving: controller, cameras, diver and gun models, bubbles, marine snow, the underwater post-process |
| `KeysAudio.*`, `KeysSfxSynth.*`, `KeysMusicSynth.*`, `KeysSynth.*` | the sound engine, effects synth and radio |
| `KeysExtras.*`, `KeysCrew.cpp`, `KeysHuman.*` | buddy boat, Luigi mode, Caelen, crew, party, shower and flags |
| `KeysTownData.*` | the town's layout, interiors, people, venues and shelves, docks, piers, estates, walkability (ported from the JS) |
| `KeysTown.*`, `KeysTownInterior.cpp`, `KeysTownBatch.cpp` | builds Boot Key Harbor City: streets, buildings, signs, props, cars, interiors, the casino, people |
| `KeysWalker.*` | going ashore: the walker, its cameras and body, interaction points |
| `KeysLandmarks.*` | bridges, marinas, Sombrero Key Light, Faro Blanco, channel marks, mooring balls, fishing piers, waterfront estates |
| `KeysGameMode.*` | wires it together; the map can stay empty |
| `Content/Python/init_unreal.py` | creates the materials and map on first launch |

Coordinates: the game works in the JS game's meters (x east, y up, z south). `KeysGeo::ToUE` turns them into Unreal centimeters, Z up.

## Online play

Online play needs the small server in `Server/` (Node.js, about 200 lines). It relays boat positions to everyone in the same crew
10 times a second and keeps each crew's leaderboard in `Server/data.json`. There are no passwords: the crew code is what keeps your
group together, so pick one strangers won't guess.

**Try it on this computer:**

    cd Server
    npm install
    npm start

To test against a server on this computer, set `ServerUrl="ws://127.0.0.1:8787"` (or launch with `-KRServer=ws://127.0.0.1:8787`).
(Friends on the same Wi-Fi can use it too: set their `ServerUrl` to `ws://YOUR-MAC'S-LAN-IP:8787`.)

**The hosted server:** it's running on Railway (project `keys-run-server`) at `wss://keys-run-server-production.up.railway.app`,
and `Config/DefaultGame.ini` already points there. Crew leaderboards are saved on a Railway volume (`/data`), so they survive redeploys.
To update it after changing `server.js`: `cd Server && railway up --service keys-run-server`.

**Host your own (Railway, free tier works):**

1. Put the `Server` folder in a GitHub repo, then on railway.app: New Project → Deploy from GitHub repo. Railway runs `npm start`.
   Or from the `Server` folder: `npm i -g @railway/cli`, `railway login`, `railway init`, `railway up`.
2. In the service's Settings → Networking, click **Generate Domain**. You get something like `keys-run-server.up.railway.app`.
3. In `Config/DefaultGame.ini`, set `ServerUrl="wss://keys-run-server.up.railway.app"` (keep the quotes; `wss://` because Railway uses HTTPS).
4. Package the game (Platforms → Mac → Package Project) and send it to your friends. Everyone types the same crew code.

You can also launch straight into a crew: `-KRLogin=Name,crew` on the command line, and `-KRServer=wss://...` to try another server.

## Testing without a keyboard

- `-KRDemo`: starts on its own in open water and drives at full throttle with gentle turns (for checking the water and wakes).
- `-KRScript="12:play;16:cast;19:hook=sailfish;21:jump;21.3:shot;25:quit"`: runs steps on a timer (seconds since launch). Steps are
  menu ids (`play`, `settings`, `controls`, `back`, `board`, `resume`, `mainmenu`, ...), `esc`, `sel=N`, `login`, `closeall`,
  `cast`, `hook=<species>`, `jump`, `catch=<species>`, `shot` (saves a screenshot with the HUD to `Saved/Screenshots`) and `quit`.
  For looking around: `tp=X,Z[,heading]`, `goto=flats|bay|hawk|reef|wall|offshore`, `cam=yaw,pitch,zoom`. For diving: `dive`, then
  `dv:hold=5` (stay at 5 m), `dv:depth=N`, `dv:look=yaw,pitch`, `dv:aimfish`, `dv:fishahead`, `dv:spear`, `dv:reload`, `dv:haul=1`,
  `dv:swim=1`, `dv:air=0.2`, `dv:chase` / `dv:mask`. `-KRScat=r,g,b` / `-KRAbs=r,g,b` override the water's scattering / absorption,
  and `-KRSeaDebug` paints the sea floor's LOD rings.
  Ashore: `tie` (bring the boat alongside the town pier and step off), `ashore`, `board`, `walk=u,v,yaw` (the town's
  own frame: u along the frontage, v inland from the shore, yaw 0 faces +u and 1.57 inland), `wlook=yaw,pitch`, `wview`,
  `use`, `buy=N`, `shopclose`, `cash=N`, `sunset`. `con=<console command>` runs any console command (`con=stat unit`),
  and `perf:town0` / `perf:town1` hide / show the town and the landmarks for frame-time comparisons.
  Boats and trolling: `ownall` (own every boat), `boat=<id>` (robalo, grady, freeman, midnight, mti, yellowfin, contender, nortech),
  `troll` / `trollin`, `trollraise` (a fish comes up on the spread now), `trollstrike` (skip the raise: a fish on now),
  `throttle=0.4`, `steer=0.3`, `reel=1` / `reel=0` (hold S).
  `buff=<key>` (diner.coffee, bar.beer, ...) as if bought at the counter.
  Events: `race` / `tourney` (as the pause-menu items), `ev:tourneylive` (skip the countdown), `ev:tourneyall` (tick off all
  five), `catch=tourney` (land the first of the five), `keep`, `harbour` (put the boat at the tournament finish).
  World: `bigmap` (the large chart), `indoorev=<EV100>` (the dark rooms' locked exposure), `pirates[=dist]` (the pirates sail
  in now from that far, 300 m by default), `banner` (a banner plane pass now).
  Test runs never write the leaderboard. Add `-RenderOffscreen` to run with no window at all.
