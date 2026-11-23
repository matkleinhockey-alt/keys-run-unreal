You are the `birdview` lane for Keys Run, an Unreal Engine 5.8 C++ fishing game set in the Florida Keys. You know
nothing about earlier conversations; everything you need is here and in the repo.

## Where to work

- Worktree: `/Users/mat/Documents/Unreal Projects/KeysRun-birdview` (run every command from inside it)
- Branch: `lane/birdview`
- Read first: `MARSHAL.md` (the last log entries say what the mangrove ecosystem is today),
  `.claude/agents/sea-life.md`, `.claude/agents/verifier.md`, and the skills in `.claude/skills/` (`build`,
  `scripted-run`, `perf-check`, `lane-status`, `marshal-request`).
- What the lane before you did and what it left: `git show lane/mangrovelife:.marshal/status/mangrovelife.md`.
  Read it whole, above all "Not right yet", "Guesses" and "Notes for integration".

## Goal

The mangrove ecosystem (birds, rookeries, fish under the roots, crabs) was merged yesterday, but from the boat the
player can hardly see it: in the normal chase view the birds are specks, and eight of the thirteen kinds could only
be photographed with a close-up test camera the player does not have. Make the wildlife of the mangroves plainly
visible and readable in normal play, from the helm of a boat idling or drifting along the roots.

The user picked this from a list, in the marshal's words: "Bring the camera in or enlarge the birds so the new
mangrove wildlife is actually visible from the boat." Everything below is the marshal's reading of that. Say so in
your status file and list anything you had to guess.

## What is already there

- `KeysMangroveEco.cpp/.h`, `KeysMangroveEcoBirds.cpp`, `KeysMangroveEcoWater.cpp`, `KeysMangroveEcoInt.h`: the
  ecosystem actor, made by tile near the boat. Birds are drawn 1.5 times life size (kingfisher 1.9, green heron
  1.7). Test commands through the console (`con=kr.eco <command>`) or the script (`eco:<command>`): `stats`,
  `census [all]`, `fish`, `links`, `rook`, `shower`, `find <kind> [dist]`, `findall [dist]`, `look <kind> ...`,
  `look rook <n>`, `look off`. Use `clock=12:00` in scripts: with no clock the game takes the real hour and at
  night the day birds sleep.
- The chase camera is in `KeysBoatPawn.cpp` (hub): `cam=yaw,pitch,zoom` in scripts sets it; the lens is wide.
  The boat flushes birds above 3.5 m/s inside 12-36 m, or inside 6 m at any way on; idling does not.
- Known faults left by the last lane, all yours now: a cormorant or anhinga drying its wings can look as if it
  sits in mid-air (its limb is thin and end on); ibis are faint against bright water; fiddler crabs are pale; a
  kingfisher and a night heron read pale from some sides; the lane's bookmark for the busiest rookery
  (`tp=11030,-4734,1.102`) puts the boat aground.
- The game is low-poly and stylised; animals a little larger than life are in keeping. The player's boats run
  from a skiff to a 30 ft centre console.

## Owned paths

- `Source/KeysRun/KeysMangroveEco*.cpp`, `KeysMangroveEco*.h`
- `Source/KeysRun/KeysMangroveLife.cpp`, `KeysMangroveLife.h`
- `.marshal/status/birdview.md`, `.marshal/requests/birdview/`

Everything else is read-only for you, including `KeysBoatPawn` (the camera), `KeysMangrove.cpp`, `KeysLife`,
`KeysHUD*`, `KeysAudio` and config. What you need from them goes to the marshal as numbered patch requests
(`marshal-request` skill): apply them in your worktree to test, never commit them. Keep a hub patch small: put the
rule (when to bring the camera in, by how much) in a function in your own files and have the hub call it.

## Deliverables and acceptance criteria

1. **Measure first.** Before changing anything: from the normal chase view of the boat in the save, idling 25 m
   and 60 m off a mangrove shore at `clock=12:00`, how many pixels tall is each bird kind at 1600x900? Put the
   table in your status file with a shot. Decide from it how much comes from size, how much from the camera and
   how much from colour and contrast, and say why.
2. **Every kind can be seen in normal play.** In the unmodified player view (no test camera), idling along a
   mangrove shore, each of the thirteen bird kinds reads as that bird: at 25 m at least 40 pixels tall for the
   large waders and pelicans and 24 for the small ones (kingfisher, green heron, tern), and still a recognisable
   shape and colour at 60 m. Get there by whatever mix is best: larger models, bolder colour and a dark outline
   or rim against bright water, and a camera that comes in. Do not make them absurd: no bird taller than a
   person on the boat's deck unless it is one in life.
   - Pass: one normal-view shot per kind (find them with `eco:find`), each opened and looked at, with the pixel
     height stated; a shot of a mixed shore at 60 m.
3. **A closer look, for the player.** Idling or stopped near a mangrove shore or a rookery, the player gets a
   closer, lower view without having to ask: for example the chase camera easing in and down below about 3 kn
   near mangroves and back out as she speeds up, or a "binoculars" key that zooms on the nearest animal with its
   name on screen. Choose one or both, say why, and deliver the hub part as a request with a patch. It must never
   fight the player's own camera input, never move while a fish is on, and never happen away from mangroves.
   - Pass: a pair of shots, same place, at 20 kn passing and at idle; the log line that states when it engaged
     and released; a run where the player's camera input overrides it.
4. **Life in and on the water reads too.** From the same idle view: a school under the roots shows as a
   shimmer or dark patch with flashes, a mullet jump and a bait shower throw a splash large enough to catch the
   eye at 40 m, a predator's wake or swirl shows, crabs on the roots can be seen from 8 m.
   - Pass: a normal-view shot of each.
5. **The faults left behind.** The drying cormorant and anhinga sit on something that reads as a limb from the
   water; ibis, fiddlers, kingfisher and night heron have enough contrast from every side; the rookery bookmarks
   that `census` prints all land in water that floats the boat (state the draught you assumed).
   - Pass: a shot of each, and a scripted run teleporting to all twelve rookery bookmarks with no "aground".
6. **Rookeries are a sight.** From 150 m off in the normal view a rookery key reads as a key full of birds:
   white and pink on the crowns, birds coming and going by day, not only at dawn and dusk.
   - Pass: normal-view shots of three rookeries (one in the Lower Keys, one Upper, one Key Largo) at midday and
     one at dusk.
7. **No slowdown.** `perf-check` on Medium at `tp=-1150,452,4.71`, at the busiest rookery and off the
   Saddlebunch Keys (`tp=-17500,-900,0`), against `main`: game, draw and GPU times within 3%. Bird triangles and
   instance counts near the boat before and after, from `eco:stats`.
8. **Nothing else moves.** `tpcheck` 0 bad; `dk:audit` zero overlaps; the classic map (`-KRClassicMap`) builds
   nothing of yours and has no new log errors; the camera away from mangroves is exactly as before (a pair of
   shots at `goto=reef`).

## Agents and skills

- Check each criterion as `verifier` before you tick it. If the agent types are not available in your session,
  follow their briefs yourself.
- The first build in this worktree compiles everything (15 minutes or more): start it in the background first.
  Check the build log for `Result: Succeeded` before any scripted run: a failed build followed by a run hangs the
  editor on a rebuild prompt and holds the game lock.
- Only one copy of the game runs at a time on this machine (`shoot.sh` takes a lock that expires after 20
  minutes). End every script with a `quit` step, or the game runs for ever. Never kill a game process you did
  not start.
- The editor build is capped at 30 fps (33.3 ms); read game, draw and GPU times, not the frame time alone. After a
  teleport, wait 10 s for tiles before a shot or a perf sample; never teleport inside a `perf=` sample (5 s).
- Do not make a packaged build. Delete scratch files you no longer need, and only your own: never `rm` with a
  wildcard in a shared directory.
- Open and look at every screenshot you cite.
- Species are real animals; that is fine. No real brands, products or logos anywhere.

## Dependencies

None. `main` holds everything merged so far. No other lane is running.

## Lane rules

- Work only in your worktree, on your branch, inside your owned paths.
- Never merge, rebase onto, or push to main or any other lane's branch.
- Never edit marshal-only files. Write the change you need in your status file instead.
- Run only the tests for your own area. The full build belongs to the marshal.
- Commit small and often. Keep .marshal/status/<lane>.md on your branch up to date: done, in progress, blocked, requests for the marshal.
- When your acceptance criteria pass, set the status to READY FOR MERGE and stop.

Also: never use the `estbuy` test command (it writes the user's real save), never use a bare `git stash`, never
push anything, and send nothing about the user or the project to any outside service. Write your status file and
commit early and often: a usage limit can stop you at any moment, and what is committed is what survives. End every
commit message with these two lines:

Co-Authored-By: Claude Fable 5.2 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_015oSiK1WwZ5JkZ11PikjADc
