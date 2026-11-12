# Keys Run (Unreal Engine 5)

An Unreal Engine 5 port of the three.js game in `~/Downloads/keys-run-deploy.zip`: center console fishing in Marathon, Florida Keys.

Nearly the whole game is ported. Everything is built in C++ at runtime, the same way the JS game builds it in code,
so there are no hand-made assets. The editor script creates the four materials and the empty map the first time you open the project.

## What's in it

- **The map:** every island, creek, channel, hump, reef line and marina from the JS game, using the same `landH` / `depthAt` /
  `zoneAt` math (`KeysGeo.cpp`), plus Miami and Biscayne Bay. US-1 and the old Seven Mile Bridge (with its missing swing span) have pilings you can hit.
  It also has palms, mangroves, conch houses, the Miami skyline, and docks at all four marinas.
- **The sea:** the same 8-wave swell and chop, and smooth Kelvin wakes (V-shaped, at the real 19.5° angle) from every boat.
  The water is colored by depth like the chart. The `M_KR_Ocean` material adds wind ripples that catch the sun, light glowing through
  thin wave tops, and foam broken into real-looking patches on breaking crests, along fresh wake arms and at the beach.
  Planing boats leave a white prop-wash trail that spreads, breaks up and fades to pale green churned water. H cycles the four sea states.
- **Boats:** all five (Robalo R230, Grady-White Canyon 306, Freeman 42LR, Midnight Express 43, MTI V42) with their real specs.
  The physics is ported line for line: 15-point buoyancy, trim and hole shot, prop ventilation, wave surfing, slams and air, beaching, reverse and neutral.
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
- **Extras:** the buddy boat (Y), the shower (J), travel to Miami and back (X), the crew, party and flags on deck, Caelen the parrot,
  and Luigi mode: trim all the way up at speed and see what happens.
- **Sky:** physically based sky, clouds, fog, and O for sunset over the Seven Mile Bridge.

- **Online with friends:** press L, type a name and a crew code, and Enter. Everyone who uses the same crew code sees each other's
  boats live (with name tags, wakes you can ride, and green dots on the chart), gets "Sam landed a 41 lb cobia!" messages, and shares a
  Crew tab on the leaderboard (B). The game remembers your login and reconnects next time; L again logs out. See "Online play" below.

Not ported: the tuna-tower view (no ported hull has a tower), the 3D screens on the dash (the electronics are a HUD panel instead),
spray particles (splashes are simple shapes), text-to-speech (the DJ and Luigi callouts are on-screen text),
engine sounds for other boats, and playing your own music files on the radio.

## Setup (Mac)

1. Install **Xcode** from the App Store, open it once, and accept the license.
2. Install the **Epic Games Launcher**, sign in, and go to Unreal Engine → Library → install **UE 5.6** (or newer).
3. Double-click `KeysRun.uproject`. When it says the KeysRun module is missing, click **Yes** to build it.
   - If it asks which engine version to use, pick the one you installed.
   - If the build fails, right-click the `.uproject` → *Generate Xcode project*, open `KeysRun.xcworkspace`, and build the `KeysRunEditor` scheme to see the errors.
4. The first time the editor opens, the Output Log shows `Keys Run: created ...` for four materials and the map `/Game/KeysRun/Maps/Keys`.
   Then open that map (it should already be open) and press **Play**.

To ship a standalone app: Platforms → Mac → Package Project.

## Controls

| Key | Action |
| --- | --- |
| A / D, Enter | choose boat, start |
| W / S | throttle / reverse (S also reels) |
| A / D | steer |
| U | gear D → N → R |
| T | trim mode (Up/Down trims); during a fight, T / Shift+T sets drag |
| Space (hold) | power a cast |
| Space | strike on the bite; hold to reel during a fight; release a landed fish |
| R | reel in |
| K | keep the fish |
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
| J | shower |
| X | travel to Miami and back |
| C / mouse wheel | zoom |
| Mouse drag | orbit the camera, or look around in first person |

## Code map

| File | What it is |
| --- | --- |
| `KeysGeo.*` | islands, depth, zones, waves, wakes, bridge pilings (ported from the JS) |
| `KeysData.*` | species, zone tables, boats |
| `KeysMesh.*` | procedural boat hulls, fish and tubes |
| `KeysWorld.*` | terrain, water surface, bridges, scenery, marinas, sky and sun |
| `KeysBoatPawn.*` | boat physics, camera, input, fishing loop |
| `KeysHUD.*` | all on-screen UI, including the chartplotter, fish finder and leaderboard |
| `KeysSave.*` | the local leaderboard save game |
| `KeysTraffic.*` | AI boat traffic and racers |
| `KeysLife.*` | birds, bird piles, weedlines, oil rigs and fish schools, and how they change the fishing |
| `KeysAudio.*`, `KeysSfxSynth.*`, `KeysMusicSynth.*`, `KeysSynth.*` | the sound engine, effects synth and radio |
| `KeysExtras.*`, `KeysCrew.cpp`, `KeysHuman.*` | buddy boat, Luigi mode, Caelen, crew, party, shower and flags |
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
  Test runs never write the leaderboard. Add `-RenderOffscreen` to run with no window at all.
