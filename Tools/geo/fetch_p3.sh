#!/bin/bash
# fetch_p3.sh <raw dir> <p3|p4|p4_bay>: the raw public data for map Phase 3 - p3 the Lower Keys (Big Pine to Key West),
# p4 Key Largo, p4_bay the strip of Florida Bay and the Gulf north of the rest of the map (fetch p3 and p4 first: p4_bay's
# OpenStreetMap files are the earlier phases' files put together, plus the two pieces north of them; its DEM is asked
# for at 1 arc-second, which is the game grid's own 10 m: 1.65 degrees of longitude at half that is 240 MB). Nothing in a request identifies the user or the project.
#   dem_<p>_half.tif       NOAA NCEI DEM mosaic (public domain), 1/2 arc-second, on the same pixel grid as dem_p1_half.tif
#   coast_<p>.json         OpenStreetMap natural=coastline (c) OpenStreetMap contributors, ODbL 1.0
#   osm_<p>_extra.json     OpenStreetMap US-1, bridges, place names, marinas, reefs, lights, runways
#   osm_<p>_mangrove.json  OpenStreetMap natural=wetland and natural=wood areas
#   osm_<p>_built.json     OpenStreetMap residential streets and residential/commercial land use (bake_mangrove.py --built, canals.py)
#   enc_<p>/*.geojson      NOAA ENC aids to navigation (public domain): fetch_enc.sh, or enc_p3_s57.py when ENC Direct is down
# A big Overpass box is asked for in strips of longitude (the public servers cut long replies short) and the strips are
# put together by merge_osm_p3.py; every reply is checked to parse as JSON with an "elements" list before it is kept.
RAW="$1"; PH="${2:-p3}"
HERE="$(cd "$(dirname "$0")" && pwd)"
PY="${PY:-/Users/mat/Documents/Unreal Projects/KeysRun-devkit/pyenv/bin/python}"
# west, south, east, north: the phase's rectangle turned back to lon/lat, with a margin. The west and north edges sit a
# whole number of 1/7200 degree pixels from dem_p1_half.tif's corner (-81.38, 24.91). SIZE = (E - W) * 7200, (N - S) * 7200.
case "$PH" in
  p3) W=-82.05; S=24.26; E=-81.22; N=24.86; SIZE=5976,4320; STRIPS="-82.05 -81.80 -81.60 -81.40 -81.22" ;;
  p4) W=-80.62; S=24.72; E=-79.98; N=25.42; SIZE=4608,5040; STRIPS="-80.62 -80.45 -80.30 -79.98" ;;
  p4_bay) W=-82.10; S=24.60; E=-80.45; N=25.30; SIZE=5940,2520; STRIPS="" ;;
  *) echo "phase p3, p4 or p4_bay"; exit 1 ;;
esac
mkdir -p "$RAW"
if [ ! -s "$RAW/dem_${PH}_half.tif" ]; then
  curl -sS -m 1200 -A "map-data-fetch/1.0" -o "$RAW/dem_${PH}_half.tif" \
    "https://gis.ngdc.noaa.gov/arcgis/rest/services/DEM_mosaics/DEM_all/ImageServer/exportImage?bbox=$W,$S,$E,$N&bboxSR=4326&imageSR=4326&size=$SIZE&format=tiff&pixelType=F32&interpolation=RSP_BilinearInterpolation&f=image" \
    -w "dem %{http_code} %{size_download}\n"
fi
osm() {
  # $1 name, $2 query with BB where the box goes
  local out="$RAW/osm_${PH}_$1.json"
  [ "$1" = coast ] && out="$RAW/coast_${PH}.json"
  [ -s "$out" ] && return
  local parts=() prev=""
  for lon in $STRIPS; do
    if [ -n "$prev" ]; then
      local part="$out.$prev.part"
      if [ ! -s "$part" ]; then
        "$HERE/fetch_osm.sh" "$part" "${2//BB/$S,$prev,$N,$lon}" || { echo "$1 $prev FAILED"; return 1; }
      fi
      parts+=("$part")
    fi
    prev="$lon"
  done
  "$PY" -I "$HERE/merge_osm_p3.py" "$out" "${parts[@]}" && rm -f "${parts[@]}"
}
if [ "$PH" = p4_bay ]; then
  # the two pieces no earlier phase's box covers: A the Gulf north of the Lower Keys, B Florida Bay up to the mainland
  piece() {
    # $1 kind, $2 piece name, $3 box (s,w,n,e), $4 query with BB
    local out="$RAW/p4_bay_$1_$2.json"
    [ -s "$out" ] && return 0
    "$HERE/fetch_osm.sh" "$out" "${4//BB/$3}" || { echo "$1 $2 FAILED"; return 1; }
  }
  QC="[out:json][timeout:200];way[\"natural\"=\"coastline\"](BB);out geom;"
  QE="[out:json][timeout:200];(nwr[\"place\"~\"^(island|islet)$\"](BB);node[\"place\"~\"^(city|town|village|hamlet|locality|suburb)$\"](BB);nwr[\"leisure\"=\"marina\"](BB);nwr[\"natural\"=\"reef\"](BB););out geom;"
  QM="[out:json][timeout:200];(way[\"natural\"=\"wetland\"](BB);way[\"natural\"=\"wood\"](BB););out geom;"
  A="24.86,-82.10,25.02,-81.41"; B="25.12,-81.45,25.30,-80.60"
  for k in coast extra mangrove; do
    case $k in coast) Q="$QC" ;; extra) Q="$QE" ;; mangrove) Q="$QM" ;; esac
    piece $k A "$A" "$Q"; piece $k B "$B" "$Q"
  done
  for k in coast extra mangrove; do
    for p in A B; do [ -s "$RAW/p4_bay_${k}_$p.json" ] || { echo "p4_bay: piece $k $p is still missing"; exit 1; }; done
  done
  join() {
    # $1 out, then the inputs that exist
    local out="$1"; shift
    local have=()
    for f in "$@"; do [ -s "$f" ] && have+=("$f"); done
    [ -s "$out" ] || "$PY" -I "$HERE/merge_osm_p3.py" "$out" "${have[@]}"
  }
  join "$RAW/coast_p4_bay.json" "$RAW/coast_p1.json" "$RAW/coast_p2.json" "$RAW/coast_p3.json" "$RAW/coast_p4.json" "$RAW/p4_bay_coast_A.json" "$RAW/p4_bay_coast_B.json"
  join "$RAW/osm_p4_bay_extra.json" "$RAW/osm_p1_extra.json" "$RAW/osm_p2_extra.json" "$RAW/osm_p3_extra.json" "$RAW/osm_p4_extra.json" "$RAW/p4_bay_extra_A.json" "$RAW/p4_bay_extra_B.json"
  join "$RAW/osm_p4_bay_mangrove.json" "$RAW/osm_p1_mangrove.json" "$RAW/osm_p2_mangrove.json" "$RAW/osm_p3_mangrove.json" "$RAW/osm_p4_mangrove.json" "$RAW/p4_bay_mangrove_A.json" "$RAW/p4_bay_mangrove_B.json"
  join "$RAW/osm_p4_bay_built.json" "$RAW/osm_p2_built.json" "$RAW/osm_p3_built.json" "$RAW/osm_p4_built.json"
  [ -s "$RAW/enc_${PH}/harbour_light.geojson" ] || "$HERE/fetch_enc.sh" "$RAW/enc_${PH}" "$W,$S,$E,$N"
  ls -l "$RAW" | grep p4_bay
  exit 0
fi
osm coast "[out:json][timeout:200];way[\"natural\"=\"coastline\"](BB);out geom;"
osm extra "[out:json][timeout:200];(way[\"highway\"~\"^trunk\"](BB);way[\"bridge\"](BB);way[\"man_made\"=\"bridge\"](BB);nwr[\"place\"~\"^(island|islet)$\"](BB);node[\"place\"~\"^(city|town|village|hamlet|locality|suburb)$\"](BB);nwr[\"leisure\"=\"marina\"](BB);nwr[\"natural\"=\"reef\"](BB);nwr[\"man_made\"=\"lighthouse\"](BB);node[\"seamark:type\"~\"^light\"](BB);way[\"aeroway\"=\"runway\"](BB););out geom;"
osm mangrove "[out:json][timeout:200];(way[\"natural\"=\"wetland\"](BB);way[\"natural\"=\"wood\"](BB);relation[\"natural\"=\"wetland\"](BB);relation[\"natural\"=\"wood\"](BB););out geom;"
osm built "[out:json][timeout:200];(way[\"highway\"~\"^(residential|living_street|unclassified)\$\"](BB);way[\"landuse\"~\"^(residential|commercial|retail)\$\"](BB);relation[\"landuse\"=\"residential\"](BB););out geom;"
[ -s "$RAW/enc_${PH}/harbour_light.geojson" ] || "$HERE/fetch_enc.sh" "$RAW/enc_${PH}" "$W,$S,$E,$N"
ls -l "$RAW" "$RAW/enc_${PH}"
