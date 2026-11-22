#!/bin/bash
# fetch_p2.sh <raw dir>: the raw public data for map Phase 2 (the Upper Keys, and the strip of Florida Bay north of
# Phase 1 that squares the map off). Nothing in a request identifies the user or the project.
#   dem_p2_half.tif       NOAA NCEI DEM mosaic (public domain), 1/2 arc-second, on the same pixel grid as dem_p1_half.tif
#   coast_p2.json         OpenStreetMap natural=coastline (c) OpenStreetMap contributors, ODbL 1.0
#   osm_p2_extra.json     OpenStreetMap US-1, bridges, place names, marinas, reefs, lights, runways
#   osm_p2_mangrove.json  OpenStreetMap natural=wetland and natural=wood areas
#   osm_p2_built.json     OpenStreetMap residential streets and residential/commercial land use (for bake_mangrove.py --built)
#   enc_p2/*.geojson      NOAA ENC Direct aids to navigation (public domain), through fetch_enc.sh
RAW="$1"
HERE="$(cd "$(dirname "$0")" && pwd)"
# west, south, east, north: Phase 2's rectangle and the bay strip, turned back to lon/lat, with a margin. The west and
# north edges sit a whole number of 1/7200 degree pixels from dem_p1_half.tif's corner (-81.38, 24.91).
W=-81.41; S=24.60; E=-80.35; N=25.12
BB="$S,$W,$N,$E"
mkdir -p "$RAW"
if [ ! -s "$RAW/dem_p2_half.tif" ]; then
  curl -sS -m 900 -A "map-data-fetch/1.0" -o "$RAW/dem_p2_half.tif" \
    "https://gis.ngdc.noaa.gov/arcgis/rest/services/DEM_mosaics/DEM_all/ImageServer/exportImage?bbox=$W,$S,$E,$N&bboxSR=4326&imageSR=4326&size=7632,3744&format=tiff&pixelType=F32&interpolation=RSP_BilinearInterpolation&f=image" \
    -w "dem %{http_code} %{size_download}\n"
fi
[ -s "$RAW/coast_p2.json" ] || "$HERE/fetch_osm.sh" "$RAW/coast_p2.json" "[out:json][timeout:200];way[\"natural\"=\"coastline\"]($BB);out geom;"
[ -s "$RAW/osm_p2_extra.json" ] || "$HERE/fetch_osm.sh" "$RAW/osm_p2_extra.json" "[out:json][timeout:200];(way[\"highway\"~\"^trunk\"]($BB);way[\"bridge\"]($BB);way[\"man_made\"=\"bridge\"]($BB);nwr[\"place\"~\"^(island|islet)$\"]($BB);node[\"place\"~\"^(city|town|village|hamlet|locality|suburb)$\"]($BB);nwr[\"leisure\"=\"marina\"]($BB);nwr[\"natural\"=\"reef\"]($BB);nwr[\"man_made\"=\"lighthouse\"]($BB);node[\"seamark:type\"~\"^light\"]($BB);way[\"aeroway\"=\"runway\"]($BB););out geom;"
[ -s "$RAW/osm_p2_mangrove.json" ] || "$HERE/fetch_osm.sh" "$RAW/osm_p2_mangrove.json" "[out:json][timeout:200];(way[\"natural\"=\"wetland\"]($BB);way[\"natural\"=\"wood\"]($BB);relation[\"natural\"=\"wetland\"]($BB);relation[\"natural\"=\"wood\"]($BB););out geom;"
[ -s "$RAW/osm_p2_built.json" ] || "$HERE/fetch_osm.sh" "$RAW/osm_p2_built.json" "[out:json][timeout:200];(way[\"highway\"~\"^(residential|living_street|unclassified)\$\"]($BB);way[\"landuse\"~\"^(residential|commercial|retail)\$\"]($BB);relation[\"landuse\"=\"residential\"]($BB););out geom;"
[ -s "$RAW/enc_p2/harbour_light.geojson" ] || "$HERE/fetch_enc.sh" "$RAW/enc_p2" "$W,$S,$E,$N"
ls -l "$RAW" "$RAW/enc_p2"
