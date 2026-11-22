"""enc_p2_s57.py <out dir>: Phase 2's aids to navigation straight from the NOAA ENC cells (S-57), for when NOAA's ENC
Direct query service (fetch_enc.sh) does not answer. Writes the same GeoJSON files fetch_enc.sh does, so bake_marks.py
runs on them unchanged:

    <out dir>/{harbour,approach}_{beacon_lateral,buoy_lateral,daymark,light}.geojson

Source: NOAA ENC (R), https://charts.noaa.gov/ENCs/ENCs.shtml. Free to download, use and redistribute under NOAA's
user agreement (https://charts.noaa.gov/ENCs/Agreement.shtml); a product made from them is not an official NOAA ENC
and is not for navigation, which is what the game's chart already says. The cells are the ones NOAA's product
catalogue (07CGD_ENCProdCat_19115.xml, 2026-11-21) lists over lon -80.97..-80.35, lat 24.60..25.12: usage band 5
(harbour, 1:12,000) and band 4 (approach, 1:45,000).

The zips are kept in <out dir>/cells and only the base cell (<cell>.000) is read out of each, in memory. The reader
below is the little of ISO 8211 / S-57 that point features need: feature records (FRID, ATTF, FSPT) and the
isolated or connected nodes they stand on (VRID, SG2D / SG3D). Nothing in a request identifies the user or project."""
import io, json, os, struct, subprocess, sys, zipfile

HARBOUR = ['US5FL5AC', 'US5FL5BC', 'US5FL5BD', 'US5FL5BE', 'US5FL5CD', 'US5FL5CE', 'US5FL5CF', 'US5FL5CH', 'US5FL5DG', 'US5FL5DH',
           'US5FL5DI', 'US5FL5EI', 'US5FL5EJ', 'US5FL5FJ', 'US5FL5FK']
APPROACH = ['US4FL1FW', 'US4FL1FX', 'US4FL1FY', 'US4FL1GW', 'US4FL1GX', 'US4FL1GY']
BOX = (-80.97, 24.60, -80.35, 25.12)
# S-57 object classes and attributes (IHO S-57 Appendix A)
OBJ = {7: 'beacon_lateral', 17: 'buoy_lateral', 39: 'daymark', 75: 'light'}
ATT = {36: 'CATLAM', 116: 'OBJNAM', 75: 'COLOUR', 2: 'BCNSHP', 4: 'BOYSHP', 107: 'LITCHR', 141: 'SIGPER', 178: 'VALNMR', 95: 'HEIGHT'}
INT_ATT = {'CATLAM', 'BCNSHP', 'BOYSHP', 'LITCHR'}


def records(b):
    """every ISO 8211 record as {tag: [field bytes, ...]} (the first, the DDR, describes the rest and is skipped)"""
    p, first = 0, True
    while p + 24 <= len(b):
        ln = int(b[p:p + 5])
        base = int(b[p + 12:p + 17])
        sl, sp, st = int(b[p + 20:p + 21]), int(b[p + 21:p + 22]), int(b[p + 23:p + 24])
        rec, q = {}, p + 24
        while b[q] != 0x1E:
            tag = b[q:q + st].decode('ascii'); q += st
            fl = int(b[q:q + sl]); q += sl
            fp = int(b[q:q + sp]); q += sp
            rec.setdefault(tag, []).append(b[p + base + fp:p + base + fp + fl - 1])    # without the field terminator
        if not first:
            yield rec
        first = False
        p += ln


def read_cell(b):
    """[(object class, lon, lat, {attribute: value})] for the point features of one base cell"""
    comf, nodes, feats = 1e7, {}, []
    for r in records(b):
        if 'DSPM' in r:
            comf = float(struct.unpack_from('<I', r['DSPM'][0], 16)[0])
        if 'VRID' in r:
            rcnm, rcid = struct.unpack_from('<BI', r['VRID'][0], 0)
            if 'SG2D' in r and len(r['SG2D'][0]) >= 8:
                y, x = struct.unpack_from('<ii', r['SG2D'][0], 0)
                nodes[(rcnm, rcid)] = (x / comf, y / comf)
        if 'FRID' in r:
            f = r['FRID'][0]
            prim, objl = f[5], struct.unpack_from('<H', f, 7)[0]
            if prim != 1 or objl not in OBJ or 'FSPT' not in r:
                continue
            att = {}
            a, q = r.get('ATTF', [b''])[0], 0
            while q + 2 <= len(a):
                code = struct.unpack_from('<H', a, q)[0]; q += 2
                e = a.find(b'\x1f', q)
                if e < 0: e = len(a)
                if code in ATT:
                    v = a[q:e].decode('latin1').strip()
                    k = ATT[code]
                    att[k] = int(v) if (k in INT_ATT and v.isdigit()) else v
                q = e + 1
            s = r['FSPT'][0]
            feats.append((objl, struct.unpack_from('<BI', s, 0), att))
    out = []
    for objl, ref, att in feats:
        if ref in nodes:
            out.append((objl, nodes[ref][0], nodes[ref][1], att))
    return out


def main():
    out = sys.argv[1]
    cells = os.path.join(out, 'cells')
    os.makedirs(cells, exist_ok=True)
    for scale, names in (('harbour', HARBOUR), ('approach', APPROACH)):
        layers = {v: [] for v in OBJ.values()}
        got = 0
        for name in names:
            zp = os.path.join(cells, name + '.zip')
            if not os.path.exists(zp) or os.path.getsize(zp) < 1000:
                subprocess.run(['curl', '-sS', '-m', '180', '-A', 'map-data-fetch/1.0', '-o', zp, f'https://charts.noaa.gov/ENCs/{name}.zip'], check=False)
            try:
                z = zipfile.ZipFile(zp)
                member = [m for m in z.namelist() if m.endswith(name + '.000')][0]
                data = z.read(member)
            except Exception as e:
                print(f'{name}: not read ({e})')
                continue
            got += 1
            for objl, lon, lat, att in read_cell(data):
                if BOX[0] <= lon <= BOX[2] and BOX[1] <= lat <= BOX[3]:
                    att['CELL'] = name
                    layers[OBJ[objl]].append(dict(type='Feature', geometry=dict(type='Point', coordinates=[round(lon, 7), round(lat, 7)]), properties=att))
        for layer, fs in layers.items():
            json.dump(dict(type='FeatureCollection', features=fs), open(os.path.join(out, f'{scale}_{layer}.geojson'), 'w'))
        print(f'{scale}: {got} of {len(names)} cells read; ' + ', '.join(f'{k} {len(v)}' for k, v in layers.items()))


if __name__ == '__main__':
    main()
