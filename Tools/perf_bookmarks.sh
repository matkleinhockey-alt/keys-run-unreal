#!/bin/bash
# Perf bookmarks: six fixed scenes, each measured on Medium then High, with a screenshot of each.
#
#   Tools/perf_bookmarks.sh <tag> [path to Keys Run.app] [only this bookmark]
#
# Runs the packaged (shipping) app once per bookmark with a -KRScript that sets the scene up, lets it settle, takes a
# clean screenshot, measures 5 s of frame times (perf=<label>: frame, game thread, draw thread, GPU - as stat unit)
# on Medium, then the same on High, and puts the quality back to Medium. Screenshots go to Docs/Screens/<tag>/,
# the numbers to Docs/Screens/<tag>/perf.tsv (one line per bookmark and quality).
# 1920x1080 windowed, so runs compare like with like.
set -u
P="$(cd "$(dirname "$0")/.." && pwd)"
TAG="${1:?tag, e.g. baseline}"
APP="${2:-$P/Saved/PerfBuild/Keys Run.app}"
ONLY="${3:-}"
EXE="$APP/Contents/MacOS/KeysRun"
SAVED="$HOME/Library/Application Support/Epic/KeysRun/Saved"
OUT="$P/Docs/Screens/$TAG"
mkdir -p "$OUT"
[ -z "$ONLY" ] && rm -f "$OUT"/*.png "$OUT/perf.tsv"

# name|setup (seconds of real time from launch; the game is up by 3 s)
BOOKMARKS=(
  "marina|5:bookmark=marina"
  "reef_underwater|5:goto=reef;7:dive;8:dv:hold=60;9:dv:depth=5;10:dv:look=3.1,-.1"
  "bait_ball|5:goto=reef;7:dive;8:dv:hold=60;9:dv:depth=4;10:dv:look=3.1,0;11:fish:bait;12:fish:look"
  "offshore_trolling|5:goto=offshore;6:throttle=0.3"
  "oil_rig|5:rig=0,140;6:cam=0,.1,1.2"
  "rain_at_sea|5:goto=offshore;6:weather=storm;7:storm=spawn:0"
  "bridge|5:tp=-2700,-560,2.6"
  "mangroves|5:tp=-1150,452,4.71"
)

for B in "${BOOKMARKS[@]}"; do
  NAME="${B%%|*}"; SETUP="${B#*|}"
  [ -n "$ONLY" ] && [ "$ONLY" != "$NAME" ] && continue
  [ -n "$ONLY" ] && [ -f "$OUT/perf.tsv" ] && grep -v "^$NAME	" "$OUT/perf.tsv" > "$OUT/perf.tmp" && mv "$OUT/perf.tmp" "$OUT/perf.tsv"
  rm -rf "$SAVED/Screenshots"
  rm -f "$SAVED/PerfLog.txt"
  SCRIPT="3:play;4:gfx=1;$SETUP;14:shotclean;15:perf=$NAME;22:gfx=2;26:shotclean;27:perf=$NAME;34:gfx=1;35:quit"
  echo "== $NAME"
  perl -e 'alarm 90; exec @ARGV' "$EXE" -windowed -ResX=1920 -ResY=1080 -KRScript="$SCRIPT" >/dev/null 2>&1
  K=0
  while IFS= read -r F; do
    [ $K -eq 0 ] && cp "$F" "$OUT/${NAME}_medium.png"
    [ $K -eq 1 ] && cp "$F" "$OUT/${NAME}_high.png"
    K=$((K + 1))
  done < <(ls "$SAVED"/Screenshots/Mac/*.png 2>/dev/null | sort)
  if [ -f "$SAVED/PerfLog.txt" ]; then cat "$SAVED/PerfLog.txt" >> "$OUT/perf.tsv"; cat "$SAVED/PerfLog.txt"; else echo "  (no perf numbers)"; fi
done
echo "screens and perf.tsv in $OUT"
