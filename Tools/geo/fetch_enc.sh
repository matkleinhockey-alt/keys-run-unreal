#!/bin/bash
# fetch_enc.sh <out dir> <west,south,east,north>: aids to navigation from NOAA ENC Direct to GIS (public domain):
# lateral beacons and buoys (the red and green marks), daymarks and lights, as GeoJSON.
OUT="$1"; BBOX="$2"
mkdir -p "$OUT"
B="https://encdirect.noaa.gov/arcgis/rest/services/encdirect"
get() {
  # $1 service, $2 layer id, $3 name
  local url="$B/$1/MapServer/$2/query?where=1%3D1&geometry=$BBOX&geometryType=esriGeometryEnvelope&inSR=4326&spatialRel=esriSpatialRelIntersects&outFields=*&outSR=4326&f=geojson"
  for try in 1 2 3; do
    curl -s -m 90 -A "map-data-fetch/1.0" "$url" -o "$OUT/$3.tmp"
    if grep -q '"features"' "$OUT/$3.tmp" 2>/dev/null; then mv "$OUT/$3.tmp" "$OUT/$3.geojson"; echo "$3 ok"; return; fi
    sleep 3
  done
  echo "$3 FAILED"; rm -f "$OUT/$3.tmp"
}
get enc_approach 3 approach_beacon_lateral
get enc_approach 8 approach_buoy_lateral
get enc_approach 11 approach_daymark
get enc_approach 13 approach_light
get enc_harbour 1 harbour_beacon_lateral
get enc_harbour 6 harbour_buoy_lateral
get enc_harbour 9 harbour_daymark
get enc_harbour 11 harbour_light
