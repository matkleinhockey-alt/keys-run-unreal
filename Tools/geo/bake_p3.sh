#!/bin/bash
# bake_p3.sh <phase> [geo dir]: one Phase 3 file (p3, p4 or p4_bay) from the raw downloads, start to finish, the same
# every time. A phase's bake reads its own marks file to cut the marked channels the grid pinches shut, and the marks
# are then placed on the new grid, so the order is fixed and starts with no marks file:
#   bake, marks, bake again (cuts the pinched legs), marks, mangroves, sites
# The earlier phases must already be in the geo dir (their edges are joined to). Bake p3, then p4, then p4_bay.
PH="$1"
HERE="$(cd "$(dirname "$0")" && pwd)"
GEO="${2:-$HERE/../../Content/KeysRun/Geo}"
RAW="${RAW:-/Users/mat/Documents/Unreal Projects/KeysRun-devkit/geo/raw}"
PY="${PY:-/Users/mat/Documents/Unreal Projects/KeysRun-devkit/pyenv/bin/python}"
OUT="${PREVIEW:-/Users/mat/Documents/Unreal Projects/KeysRun-devkit/geo}"
case "$PH" in p3) SRC=p3 ;; p4|p4_bay) SRC=p4 ;; *) echo "phase p3, p4 or p4_bay"; exit 1 ;; esac
[ "$PH" = p4_bay ] && SRC="${BAY_SRC:-p4_bay}"
rm -f "$GEO/${PH}_marks.txt" "$GEO/${PH}_canals.txt"
set -e
"$PY" "$HERE/bake.py" "$PH" "$RAW" "$GEO" > /dev/null
"$PY" "$HERE/bake_marks.py" "$PH" "$RAW/enc_$SRC" "$GEO" > /dev/null
"$PY" "$HERE/bake.py" "$PH" "$RAW" "$GEO" --preview "$OUT/${PH}_game.png"
"$PY" "$HERE/bake_marks.py" "$PH" "$RAW/enc_$SRC" "$GEO"
"$PY" "$HERE/bake_mangrove.py" "$PH" "$RAW/osm_${SRC}_mangrove.json" "$GEO" --built "$RAW/osm_${SRC}_built.json" --preview "$OUT/${PH}_mangrove.png"
"$PY" "$HERE/sites_p3.py" "$PH" "$GEO" "$RAW"
