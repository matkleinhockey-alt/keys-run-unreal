#!/bin/bash
# fetch_p3.sh <raw dir> <p3|p4>: the raw public data for map Phase 3 - p3 the Lower Keys (Big Pine to Key West), p4 Key
# Largo and the strip of Florida Bay north of the rest of the map. Nothing in a request identifies the user or the project.
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
  *) echo "phase p3 or p4"; exit 1 ;;
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
osm coast "[out:json][timeout:200];way[\"natural\"=\"coastline\"](BB);out geom;"
osm extra "[out:json][timeout:200];(way[\"highway\"~\"^trunk\"](BB);way[\"bridge\"](BB);way[\"man_made\"=\"bridge\"](BB);nwr[\"place\"~\"^(island|islet)$\"](BB);node[\"place\"~\"^(city|town|village|hamlet|locality|suburb)$\"](BB);nwr[\"leisure\"=\"marina\"](BB);nwr[\"natural\"=\"reef\"](BB);nwr[\"man_made\"=\"lighthouse\"](BB);node[\"seamark:type\"~\"^light\"](BB);way[\"aeroway\"=\"runway\"](BB););out geom;"
osm mangrove "[out:json][timeout:200];(way[\"natural\"=\"wetland\"](BB);way[\"natural\"=\"wood\"](BB);relation[\"natural\"=\"wetland\"](BB);relation[\"natural\"=\"wood\"](BB););out geom;"
osm built "[out:json][timeout:200];(way[\"highway\"~\"^(residential|living_street|unclassified)\$\"](BB);way[\"landuse\"~\"^(residential|commercial|retail)\$\"](BB);relation[\"landuse\"=\"residential\"](BB););out geom;"
[ -s "$RAW/enc_${PH}/harbour_light.geojson" ] || "$HERE/fetch_enc.sh" "$RAW/enc_${PH}" "$W,$S,$E,$N"
ls -l "$RAW" "$RAW/enc_${PH}"
