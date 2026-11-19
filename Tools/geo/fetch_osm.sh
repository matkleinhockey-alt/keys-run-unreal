#!/bin/bash
# fetch_osm.sh <out.json> <overpass query>: tries the public Overpass mirrors until one returns JSON
out="$1"; q="$2"
for try in 1 2 3; do
  for h in https://overpass-api.de/api/interpreter https://overpass.private.coffee/api/interpreter https://overpass.kumi.systems/api/interpreter; do
    code=$(curl -sS -m 240 -A "map-data-fetch/1.0" -o "$out.tmp" --data-urlencode "data=$q" "$h" -w "%{http_code}")
    if [ "$code" = 200 ] && head -c 1 "$out.tmp" | grep -q '{'; then mv "$out.tmp" "$out"; echo "ok $h $(wc -c < "$out")"; exit 0; fi
    echo "fail $h $code"; sleep 8
  done
done
exit 1
