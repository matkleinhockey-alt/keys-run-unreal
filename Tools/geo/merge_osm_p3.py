"""merge_osm_p3.py <out.json> <part.json> ...: put Overpass replies for neighbouring boxes together as one, each element
once (a way or relation that crosses a box edge comes back whole in both replies). Fails if a part does not parse or has
no "elements" list, or carries Overpass's "remark" (a reply cut short by a timeout)."""
import json, sys

out, parts = sys.argv[1], sys.argv[2:]
seen, els = set(), []
for p in parts:
    d = json.load(open(p))
    if not isinstance(d.get('elements'), list):
        sys.exit(f'{p}: no elements')
    if d.get('remark'):
        sys.exit(f'{p}: cut short: {d["remark"]}')
    for e in d['elements']:
        k = (e['type'], e['id'])
        if k not in seen:
            seen.add(k)
            els.append(e)
json.dump(dict(elements=els), open(out, 'w'), separators=(',', ':'))
print(f'{out}: {len(els)} elements from {len(parts)} parts')
