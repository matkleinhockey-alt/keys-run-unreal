---
name: geo-bake
description: Fetch and bake real map data for a Keys Run map phase (elevation grid, chart marks, estate plat, mangrove cover). Use when adding a map phase or re-baking one.
---

# Geo bake

Python: `"/Users/mat/Documents/Unreal Projects/KeysRun-devkit/pyenv/bin/python"`. Raw downloads:
`/Users/mat/Documents/Unreal Projects/KeysRun-devkit/geo/raw` (outside the repo). Baked output:
`Content/KeysRun/Geo`.

| Step | Script | Output |
|---|---|---|
| Chart marks, raw | `Tools/geo/fetch_enc.sh <out dir> <west,south,east,north>` | GeoJSON of beacons, buoys, lights |
| OSM, raw | `KeysRun-devkit/geo/fetch_osm.sh <out.json> <overpass query>` (tries the Overpass mirrors in turn) | JSON |
| Marks | `Tools/geo/bake_marks.py <phase> <enc dir> <geo dir>` | `<phase>_marks.txt` |
| Estate lots | `Tools/geo/plat.py <phase> <geo dir> [--preview png]` | `<phase>_plat.txt` |
| Mangroves | `Tools/geo/bake_mangrove.py <phase> <osm json> <geo dir> [--preview png]` | `<phase>_mangrove.bin` |

`<phase>.kgeo` (the 10 m height and shore grid with island rings) must exist before the other three run; `kgeo.py`
reads it.

## Rules

- The frame (1:3 compression, 19 degree rotation, Vaca Key at game -120, 0) comes from `frame.py`. Never redefine it.
- Outside requests carry no personal or project details: User-Agent `map-data-fetch/1.0`, nothing else identifying.
- Always pass `--preview` where a script has it and look at the picture.
- Paste each script's summary line into your status file.
- Check the result in the game with `scripted-run` (`bigmap` shows the chart).
