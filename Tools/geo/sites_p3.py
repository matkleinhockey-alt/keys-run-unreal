"""sites_p3.py <phase> <geo dir> <raw dir>: the named places of a Phase 3 file, for the game to build on.

Reads <phase>.kgeo and <phase>_places.json (bake.py) and the raw OSM extras, writes <geo dir>/<phase>_sites.txt, which
KeysGeoReal loads (KeysGeoReal::Sites, RoadX0, RoadX1). One place a line:

    kind|name|x|z|x2|z2|value

  us1     where US-1 really runs on this phase's stretch of the chain line: from x to x2 (west of Key West, and past
          where the road leaves Key Largo, the chain line is only the keys' axis and carries no road or bridge)
  light   a reef light; value = the depth it stands in
  reef    a reef by name, on its crest; value = the depth there (only crests the grid has under 9 m)
  marina  a marina site; x, z on the shore, x2, z2 the nearest open water with 1.8 m under it, value = that depth.
          The names are INVENTED here (MARINA_NAMES): OpenStreetMap carries the real businesses' names, which the
          project does not use. They are dealt out west to east, so they do not change unless the list does.
  bridge  a road bridge that is not on the chain line, from x, z to x2, z2
  town    a town or village by name (place names are not brands)
  spot    somewhere worth a teleport: open water, value = a heading in radians

Nothing here is random."""
import json, math, os, sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import frame as F
from kgeo import KGeo

# invented: no real marina, shop or brand carries these (checked against the names in the OSM extras when run)
MARINA_NAMES = {
    # west to east as the bake finds them: Key West, four on Stock Island, Cudjoe, Summerland, Big Pine
    'p3': ['Wreckers Wharf Marina', 'Salt Pond Basin', 'Shrimp Road Docks', 'Cow Key Fuel Dock', 'Boca Chica Basin', 'Cudjoe Cut Docks',
           'Summerland Fuel and Bait', 'Spanish Harbor Docks', 'Saddlebunch Landing', 'Bow Channel Boatworks', 'Niles Channel Landing', 'Torch Key Landing'],
    'p4': ['Rock Harbor Marina', 'Mandalay Flats Dock', 'Largo Sound Landing', 'Garden Cove Basin', 'Jewfish Creek Fuel Dock', 'Blackwater Sound Marina',
           'Point Lowe Docks', 'Molasses Run Marina', 'Newport Cut Basin', 'Lake Largo Landing', 'Pirate Lagoon Docks', 'Adams Cut Marina',
           'Steamboat Creek Dock', 'Card Sound Landing', 'Dispatch Creek Basin', 'Angelfish Creek Docks', 'Rattlesnake Key Landing', 'Sunset Cove Marina',
           'Tarpon Basin Docks', 'Dusenbury Creek Landing', 'Elbow Light Marina', 'Carysfort Yacht Basin', 'Garden Cove Fuel and Bait', 'Sexton Cove Dock'],
    'p4_bay': ['Flamingo Basin', 'Buttonwood Landing'],
}
# teleport spots asked for by name: (name, lon, lat, heading). Each is moved to the nearest open water.
SPOTS = {
    'p3': [('Off Big Pine Key', -81.3500, 24.6450, 0.0), ('Off Sugarloaf Key', -81.5450, 24.5950, 0.0), ('Key West Harbor', -81.8130, 24.5600, 1.57),
           ('Off Boca Chica', -81.6900, 24.5500, 0.0), ('Northwest Channel', -81.8500, 24.6000, 0.0)],
    'p4': [('Off Rock Harbor', -80.4300, 25.0650, 0.0), ('Largo Sound', -80.3900, 25.1330, 0.0), ('Off Carysfort Reef', -80.2200, 25.2230, 0.0),
           ('Blackwater Sound', -80.4300, 25.1700, 0.0), ('Barnes Sound', -80.3800, 25.2500, 0.0)],
    'p4_bay': [('Off Flamingo', -80.9200, 25.1150, 0.0)],
}


def main():
    phase, geo, raw = sys.argv[1:4]
    g = KGeo(os.path.join(geo, phase + '.kgeo'))
    pl = json.load(open(os.path.join(geo, phase + '_places.json')))
    x0, z0, c = g.x0, g.z0, g.cell
    x1, z1 = x0 + (g.nx - 1) * c, z0 + (g.nz - 1) * c
    inside = lambda x, z, m=200: x0 + m < x < x1 - m and z0 + m < z < z1 - m

    def open_water(x, z, depth=1.8, off=25.0, reach=600.0):
        """the nearest cell with that much water, that far off the shore"""
        j, i = int(round((z - z0) / c)), int(round((x - x0) / c))
        r = int(reach / c)
        ja, jb, ia, ib = max(0, j - r), min(g.nz, j + r + 1), max(0, i - r), min(g.nx, i + r + 1)
        ok = (-g.height[ja:jb, ia:ib] >= depth) & (g.shore[ja:jb, ia:ib] >= off)
        if not ok.any():
            return None
        jj, ii = np.nonzero(ok)
        k = int(np.argmin((jj + ja - j) ** 2 + (ii + ia - i) ** 2))
        px, pz = x0 + (ii[k] + ia) * c, z0 + (jj[k] + ja) * c
        return px, pz, float(-g.height[jj[k] + ja, ii[k] + ia])

    out = []
    # ---- the road: US-1's own extent on the chain line (PHASES 'road' in bake.py writes it into the places file)
    road = pl.get('road')
    if road:
        out.append(('us1', 'US-1', road[0], 0, road[1], 0, 0))
    for l in pl.get('lights', []):
        out.append(('light', l['name'], l['pts'][0], l['pts'][1], 0, 0, l['depth']))
    seen = set()
    for r in pl.get('reef_sites', []):
        if r['name'] and r['name'] not in seen and 0 < r['depth'] < 9 and inside(*r['pts']):
            seen.add(r['name'])
            out.append(('reef', r['name'], r['pts'][0], r['pts'][1], 0, 0, r['depth']))
    # ---- marinas: the sites, west to east, each with an invented name and its open water
    real_names = set(e.get('tags', {}).get('name', '').strip().lower() for e in json.load(open(os.path.join(raw, f'osm_{pl.get("src", phase)}_extra.json')))['elements']
                     if e.get('tags', {}).get('leisure') == 'marina')
    sites = []
    for m in pl.get('marinas', []):
        p = m['pts']
        x, z = (sum(p[0::2]) / (len(p) // 2), sum(p[1::2]) / (len(p) // 2))
        real_names.add(m.get('name', '').strip().lower())
        if not inside(x, z):
            continue
        w = open_water(x, z)
        if w and not any(math.hypot(x - s[0], z - s[1]) < 260 for s in sites):     # (two marinas a block apart are one stop)
            sites.append((x, z) + w)
    sites.sort()
    names = MARINA_NAMES.get(phase, [])
    clash = [n for n in names if n.lower() in real_names]
    assert not clash, f'invented names that are real ones here: {clash}'
    for k, (x, z, wx, wz, d) in enumerate(sites[:len(names)]):
        out.append(('marina', names[k], x, z, wx, wz, d))
    for b in pl.get('side_bridges', []):
        out.append(('bridge', b['name'], b['a'][0], b['a'][1], b['b'][0], b['b'][1], 0))
    for t in pl.get('localities', []):
        if t['kind'] in ('city', 'town', 'village') and inside(*t['pts'], m=0):
            out.append(('town', t['name'], t['pts'][0], t['pts'][1], 0, 0, 0))
    for name, lon, lat, hd in SPOTS.get(phase, []):
        x, z = F.to_game(lon, lat)
        w = open_water(x, z, depth=2.2, off=60.0, reach=1500.0) if inside(x, z) else None
        if w:
            out.append(('spot', name, w[0], w[1], 0, 0, hd))
    path = os.path.join(geo, phase + '_sites.txt')
    with open(path, 'w') as f:
        f.write('# named places (Tools/geo/sites_p3.py): kind|name|x|z|x2|z2|value. Marina names are invented.\n')
        for k, name, a, b, a2, b2, v in out:
            f.write('%s|%s|%.1f|%.1f|%.1f|%.1f|%.2f\n' % (k, name, a, b, a2, b2, v))
    n = {}
    for o in out:
        n[o[0]] = n.get(o[0], 0) + 1
    print(f'{phase} sites: ' + ', '.join(f'{v} {k}' for k, v in sorted(n.items())) + f' ({len(sites)} marina sites found, {len(names)} names) -> {path}'
          + (f'; US-1 on the chain from x {road[0]:.0f} to {road[1]:.0f}' if road else ''))


if __name__ == '__main__':
    main()
