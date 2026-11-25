# smooth

Status: IN PROGRESS (criteria 1 and 2 done; 3 needs R2 from the marshal; 4 partly verified)
Updated: 2026-11-25
Branch: lane/smooth (see git log)

The criteria are the marshal's reading of the user's remarks ("goes dark the screen" on surfacing; the trolling strike
"smoother and less pixelated"). Evidence images: `/Users/mat/Documents/Unreal Projects/KeysRun-devkit/lanes/KeysRun-smooth/evidence/`.
Logs/shots: same folder's `log_<tag>.log`, `shots/<tag>/`. "Before" runs use the lane build with the test switches
`dv:exfix=0` / `strikefix=0` (old code paths), except J-from-far, which is now a cut in both.

## Acceptance criteria
- [x] 1. Surfacing reads right. CAUSE: the underwater post material (M_KR_Underwater) divides its haze by the eye
  adaptation, so with auto exposure free under water it fed on itself: exposure ran from 1.5 to 0.05 (-4.7 EV) in 10 s
  (KRSurf log, log_surfC.log, `exposure ... (x EV from deck)`), and the deck took ~7 s to climb back. Switch: `dv:exfix=0`
  brings it back; `con=r.EyeAdaptationQuality 0` removed the slow recovery (surfA). FIX (KeysDiver): under water the
  exposure is held at the one last measured on deck (a view-extension probe reads it); the -0.1 bias applies only while
  the camera is under; ExitToBoat snaps the underwater look off and marks a camera cut; J from far off is a cut, not a
  0.8 s fade through black; a blackout haul-out ends in the pawn's tick (no stale underwater frame); the mask lens is kept
  0.2 m clear of the waves (it drew a black slab over bare sand when it sat in a crest, surfE shot 76).
  Mean brightness (0-255), steady deck 146-149:
  J from 6 m before 31,10,26,40,79,142 (+0.25..+7 s) / after 150,149,149,148,149 (+0.1..+4 s);
  J from 18 m before 87,80,73,75,88,100,140 / after 148,148,148,147,147,146,146;
  blackout haul-out before 89,35,12,15,36,119 after waking / after 146,146,146,146,147;
  swim-up crossing 114 under -> 160-170 floating (view toward the sun) -> J 147-150 (surfI); over the side 138-153 (surfI).
  Strips: c1_J6m_*, c1_J18m_*, c1_blackout_*, c1_swimup_after, c1_overside_after, c1_swimup_waterline_bug.
- [x] 2. The strike, smooth. Before (strikefix=0, ftB2): the hit is a 61 m / 74 deg hard cut from the raise shot to the
  chair shot; the chair jump-zoom swings 10-13 deg a frame to a 24 deg lens on a fish 68 m out; the raise shot jolts
  1-1.3 m a frame as each swell crest pushes the camera floor; the first jumps arc over flat sea level, under the
  offshore swell (fish 1.6-2.2 m below the surface while "jumping"), and snap the fish down by the swell on takeoff.
  After: a strike camera stays out with the fish (beyond it from the boat, so the boat is in the frame) on damped springs
  through the hit and the first jump; one deliberate cut to the chair (TSR history dropped); the chair aim and lens ease
  toward the fish and glide when it changes sides; 1.3 s glide into first person; gentler first-person jump swing in the
  chair; jumps leave and re-enter at the swell; the shake is a smooth wobble, not a new random offset a frame; the chase
  camera's wave floor is eased. Max turn in third person: 1 deliberate cut (170 deg), otherwise <= 7 deg a frame.
  Frame times (1600x900 Medium, evidence/c2_frametimes.txt): third person before 0 and 0 frames over 50 ms, after 1 (54 ms)
  and 0; first person before 7 and 68, after 69 and 7 (render thread ~75 ms in the chair first-person view on both).
  Stills: c2_strike_before_strkC_2560_medium.png (old, raise phase only: the shots slowed it), c2_strike_after_strkD/E.
- [ ] 3. The strike, sharp. CAUSE FOUND, fix is marshal-only: request R2 (the splash's white water through the wash-streak
  warp in OCEAN_FOAM aliases into nested squares). Proof run strkG (splash wash off locally, reverted): clean. Not
  checked at High. My camera changes keep the shots further off the water but do not remove it.
- [ ] 4. Nothing else moves. Done: real map tpcheck 48, 0 bad; classic map 19, 0 bad; cast + hook + underwater fish cam
  + jump + landing close-up + keep; dive, dv:aimfish (85 targets), dv:spear (goliath 146.7 lb), haul, card
  (regR, c4_cast_fishcam_landing_spear_regR.png). Perf at goto=offshore trolling, Medium 1600x900 (perf=, 5 s):
  before 33.34/33.34/34.14 ms frame, game 13.3/16.6/15.7, GPU 28.3/28.3/27.8; after 36.43/35.24/35.45, game 19.8/19.0/15.1,
  GPU 27.8/28.8/30.1. Every "after" sample (and pfB3) holds the gull's text-to-speech stall (KRSpeech at the same frame,
  155-463 ms); the code that runs while trolling idle is a few float ops a frame. NOT settled within 3%: needs a quiet
  sample without the gull's line (marshal).

## Done
- KeysDiver.cpp/.h: exposure probe + hold under water, ExitToBoat cut/snap, waterline lens rule, blackout haul-out via
  the pawn; test hooks dv:surflog=1 (KRSurf lines), dv:exfix=0, dv:exopen=<EV>.
- KeysBoatPawn.cpp/.h: J from far off is a cut; blackout haul-out; strike camera, chair aim/lens easing, side-flip glide,
  the one cut, first-person glide; swell-riding jumps; smooth shake; eased chase floor; test hooks strikefix=0,
  ftlog=<s> (KRStrike per-frame lines), trollraise=<species>.

## Requests for the marshal
- R2 (open): Content/Python/init_unreal.py OCEAN_FOAM (or KeysLife Splash) - the pixelated water at the strike,
  see .marshal/requests/smooth/R2.md.

## Notes for integration
- Guesses: "pixelated" read as the splash water's aliasing plus jerky camera moves (the fish model itself looked fine
  at 56%); the strike cam's framing (beyond the fish, boat in frame) and the one cut to the chair are my design; J far
  off now cuts instead of fading through black (the criterion forbids darker frames); exposure under water equals the
  deck's (the reef under water is no longer silhouetted after ~10 s, as it was through the runaway) - a visible change.
- The shake change also affects a cast fish's strike (smooth wobble instead of per-frame noise), and the chase floor
  easing applies to every chase view.
- The underwater fish cam on a cast mahi showed open water without the fish (regR 50-52); not compared with main.
- Not verified: High quality for 2/3; packaged build; sound.
