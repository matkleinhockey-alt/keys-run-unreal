# unmeshopt.py in.glb out.glb : decodes EXT_meshopt_compression, KHR_mesh_quantization and EXT_texture_webp
# into a plain glTF 2.0 GLB (float attributes, PNG textures) that Unreal's glTF importer reads.
import json, struct, sys, subprocess, os, math, tempfile

def read_glb(path):
    b = open(path, 'rb').read()
    assert b[:4] == b'glTF'
    off = 12
    js = None; bin_ = b''
    while off < len(b):
        ln, ty = struct.unpack('<II', b[off:off + 8])
        chunk = b[off + 8:off + 8 + ln]
        if ty == 0x4E4F534A: js = json.loads(chunk)
        elif ty == 0x004E4942: bin_ = chunk
        off += 8 + ln
    return js, bin_

# ---------------------------------------------------------------- vertex codec (version 0)
def decode_bytes_group(data, pos, bits, out):
    if bits == 0:
        out.extend(b'\x00' * 16); return pos
    if bits == 8:
        out.extend(data[pos:pos + 16]); return pos + 16
    nb = 16 * bits // 8
    packed = data[pos:pos + nb]
    var = pos + nb
    mask = (1 << bits) - 1
    vals = []
    for byte in packed:
        for k in range(8 // bits):
            vals.append((byte >> (8 - bits * (k + 1))) & mask)
    for v in vals:
        if v == mask:
            out.append(data[var]); var += 1
        else:
            out.append(v)
    return var

def decode_vertex_buffer(data, count, stride):
    assert data[0] == 0xa0, hex(data[0])
    tail = max(stride, 32)
    last = bytearray(data[len(data) - stride:])
    out = bytearray(count * stride)
    block = min((8192 // stride) & ~15, 256)
    pos = 1
    v0 = 0
    while v0 < count:
        n = min(block, count - v0)
        groups = (n + 15) // 16
        hdr_size = (groups + 3) // 4
        for k in range(stride):
            hdr = data[pos:pos + hdr_size]
            pos += hdr_size
            buf = bytearray()
            for g in range(groups):
                bl = (hdr[g // 4] >> ((g % 4) * 2)) & 3
                bits = [0, 2, 4, 8][bl]
                pos = decode_bytes_group(data, pos, bits, buf)
            p = last[k]
            for i in range(n):
                d = buf[i]
                d = (d >> 1) ^ (-(d & 1) & 0xff)
                p = (p + d) & 0xff
                out[(v0 + i) * stride + k] = p
            last[k] = p
        v0 += n
    assert pos <= len(data) - tail + 0, (pos, len(data), tail)
    return out

# ---------------------------------------------------------------- index codec (versions 0, 1)
def decode_index_buffer(data, count):
    assert data[0] & 0xf0 == 0xe0
    version = data[0] & 0x0f
    ntri = count // 3
    code = data[1:1 + ntri]
    p = [1 + ntri]
    aux = data[len(data) - 16:]
    edge = [[0xffffffff, 0xffffffff] for _ in range(16)]
    vf = [0xffffffff] * 16
    eo = [0]; vo = [0]
    nxt = 0; last = 0
    out = []
    def vbyte():
        r = 0; sh = 0
        while True:
            b = data[p[0]]; p[0] += 1
            r |= (b & 0x7f) << sh
            if b < 128: return r
            sh += 7
    def dec_index(lastv):
        v = vbyte()
        d = (v >> 1) ^ -(v & 1)
        return (lastv + d) & 0xffffffff
    def push_v(v, cond=True):
        vf[vo[0]] = v
        if cond: vo[0] = (vo[0] + 1) & 15
    def push_e(a, b):
        edge[eo[0]] = [a, b]; eo[0] = (eo[0] + 1) & 15
    fecmax = 13 if version >= 1 else 15
    for i in range(ntri):
        ct = code[i]
        if ct < 0xf0:
            fe = ct >> 4
            a, b = edge[(eo[0] - 1 - fe) & 15]
            fec = ct & 15
            if fec < fecmax:
                if fec == 0:
                    c = nxt; nxt += 1
                    push_v(c, True)
                else:
                    c = vf[(vo[0] - 1 - fec) & 15]
                    push_v(c, False)
                out += [a, b, c]
                push_e(c, b); push_e(a, c)
            else:
                if fec != 15:
                    c = (last + (fec - (fec ^ 3))) & 0xffffffff
                else:
                    c = dec_index(last)
                last = c
                out += [a, b, c]
                push_v(c)
                push_e(c, b); push_e(a, c)
        else:
            if ct < 0xfe:
                ca = aux[ct & 15]
                feb = ca >> 4; fec = ca & 15
                a = nxt; nxt += 1
                if feb == 0: b = nxt; nxt += 1
                else: b = vf[(vo[0] - feb) & 15]
                if fec == 0: c = nxt; nxt += 1
                else: c = vf[(vo[0] - fec) & 15]
                out += [a, b, c]
                push_v(a); push_v(b, feb == 0); push_v(c, fec == 0)
                push_e(b, a); push_e(c, b); push_e(a, c)
            else:
                ca = data[p[0]]; p[0] += 1
                fea = 0 if ct == 0xfe else 15
                feb = ca >> 4; fec = ca & 15
                if ca == 0: nxt = 0
                if fea == 0: a = nxt; nxt += 1
                else: a = 0
                if feb == 0: b = nxt; nxt += 1
                else: b = vf[(vo[0] - feb) & 15]
                if fec == 0: c = nxt; nxt += 1
                else: c = vf[(vo[0] - fec) & 15]
                if fea == 15: last = a = dec_index(last)
                if feb == 15: last = b = dec_index(last)
                if fec == 15: last = c = dec_index(last)
                out += [a, b, c]
                push_v(a); push_v(b, feb == 0 or feb == 15); push_v(c, fec == 0 or fec == 15)
                push_e(b, a); push_e(c, b); push_e(a, c)
    return out

# ---------------------------------------------------------------- filters
def filter_oct(buf, count, stride):
    if stride == 4:
        fmt, mx = 'b', 127.0
    else:
        fmt, mx = 'h', 32767.0
    n = count * 4
    v = list(struct.unpack('<%d%s' % (n, fmt), bytes(buf)))
    for i in range(count):
        x = float(v[i * 4]); y = float(v[i * 4 + 1]); z = float(v[i * 4 + 2]) - abs(x) - abs(y)
        t = z if z < 0 else 0.0
        x += t if x >= 0 else -t
        y += t if y >= 0 else -t
        l = math.sqrt(x * x + y * y + z * z) or 1.0
        s = mx / l
        v[i * 4] = int(round(x * s)); v[i * 4 + 1] = int(round(y * s)); v[i * 4 + 2] = int(round(z * s))
    return bytearray(struct.pack('<%d%s' % (n, fmt), *v))

def filter_quat(buf, count):
    n = count * 4
    v = list(struct.unpack('<%dh' % n, bytes(buf)))
    scale = 1.0 / math.sqrt(2.0)
    for i in range(count):
        sf = v[i * 4 + 3] | 3
        ss = scale / float(sf)
        x = v[i * 4] * ss; y = v[i * 4 + 1] * ss; z = v[i * 4 + 2] * ss
        ww = 1.0 - x * x - y * y - z * z
        w = math.sqrt(ww if ww > 0 else 0.0)
        qc = v[i * 4 + 3] & 3
        r = [0, 0, 0, 0]
        r[(qc + 1) & 3] = int(round(x * 32767)); r[(qc + 2) & 3] = int(round(y * 32767)); r[(qc + 3) & 3] = int(round(z * 32767)); r[qc] = int(round(w * 32767))
        v[i * 4:i * 4 + 4] = r
    return bytearray(struct.pack('<%dh' % n, *v))

def filter_exp(buf, count, stride):
    n = count * stride // 4
    v = struct.unpack('<%di' % n, bytes(buf))
    f = [math.ldexp(float((x << 8) >> 8 if False else ((x & 0xffffff) - (1 << 24) if x & 0x800000 else (x & 0xffffff))), (x >> 24)) for x in v]
    return bytearray(struct.pack('<%df' % n, *f))

# ---------------------------------------------------------------- main
COMP = {5120: ('b', 1, 127.0), 5121: ('B', 1, 255.0), 5122: ('h', 2, 32767.0), 5123: ('H', 2, 65535.0), 5125: ('I', 4, 0), 5126: ('f', 4, 0)}
NCOMP = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4, 'MAT4': 16}

def main(inp, outp):
    js, binb = read_glb(inp)
    # decode every bufferView into raw bytes
    views = []
    for bv in js['bufferViews']:
        e = bv.get('extensions', {}).get('EXT_meshopt_compression')
        if e:
            src = binb[e.get('byteOffset', 0):e.get('byteOffset', 0) + e['byteLength']]
            cnt, st = e['count'], e['byteStride']
            if e['mode'] == 'ATTRIBUTES':
                d = decode_vertex_buffer(src, cnt, st)
                flt = e.get('filter', 'NONE')
                if flt == 'OCTAHEDRAL': d = filter_oct(d, cnt, st)
                elif flt == 'QUATERNION': d = filter_quat(d, cnt)
                elif flt == 'EXPONENTIAL': d = filter_exp(d, cnt, st)
            elif e['mode'] == 'TRIANGLES':
                idx = decode_index_buffer(src, cnt)
                d = bytearray(struct.pack('<%d%s' % (cnt, 'H' if st == 2 else 'I'), *idx))
            else:
                raise Exception('mode ' + e['mode'])
            views.append((d, st))
        else:
            o = bv.get('byteOffset', 0)
            views.append((bytearray(binb[o:o + bv['byteLength']]), bv.get('byteStride', 0)))
    out = bytearray()
    newviews = []
    def add_view(data, target=None):
        while len(out) % 4: out.append(0)
        v = {'buffer': 0, 'byteOffset': len(out), 'byteLength': len(data)}
        if target: v['target'] = target
        out.extend(data)
        newviews.append(v)
        return len(newviews) - 1
    index_accs = set()
    for m in js.get('meshes', []):
        for pr in m['primitives']:
            if 'indices' in pr: index_accs.add(pr['indices'])
    joint_accs = set()
    for m in js.get('meshes', []):
        for pr in m['primitives']:
            for k, a in pr['attributes'].items():
                if k.startswith('JOINTS'): joint_accs.add(a)
    # accessors: re-pack, dequantize to float (except indices / joints)
    for ai, a in enumerate(js['accessors']):
        if 'bufferView' not in a: continue
        d, st = views[a['bufferView']]
        fmt, sz, mx = COMP[a['componentType']]
        nc = NCOMP[a['type']]
        es = sz * nc
        st = st or es
        o = a.get('byteOffset', 0)
        vals = []
        for i in range(a['count']):
            vals.extend(struct.unpack_from('<%d%s' % (nc, fmt), d, o + i * st))
        if ai in index_accs:
            a['bufferView'] = add_view(struct.pack('<%d%s' % (len(vals), fmt), *vals), 34963)
        elif ai in joint_accs:
            a['bufferView'] = add_view(struct.pack('<%dH' % len(vals), *vals), 34962)
            a['componentType'] = 5123
        else:
            if a['componentType'] != 5126:
                if a.get('normalized'):
                    vals = [max(-1.0, v / mx) for v in vals]
                else:
                    vals = [float(v) for v in vals]
                a['componentType'] = 5126
            a.pop('normalized', None)
            a['bufferView'] = add_view(struct.pack('<%df' % len(vals), *vals))
            if a['type'] in ('VEC3', 'VEC2', 'VEC4', 'SCALAR') and ('min' in a or 'max' in a or True):
                if a['count']:
                    a['min'] = [min(vals[c::nc]) for c in range(nc)]
                    a['max'] = [max(vals[c::nc]) for c in range(nc)]
        a.pop('byteOffset', None)
    # images: WebP -> PNG
    tmp = tempfile.mkdtemp(dir=os.path.dirname(os.path.abspath(outp)))
    for ii, im in enumerate(js.get('images', [])):
        d, _ = views[im['bufferView']]
        if im.get('mimeType') == 'image/webp':
            wp = os.path.join(tmp, 'i%d.webp' % ii); pp = os.path.join(tmp, 'i%d.png' % ii)
            open(wp, 'wb').write(d)
            subprocess.check_call(['sips', '-s', 'format', 'png', wp, '--out', pp], stdout=subprocess.DEVNULL)
            d = open(pp, 'rb').read()
            os.remove(wp); os.remove(pp)
            im['mimeType'] = 'image/png'
        im['bufferView'] = add_view(d)
    os.rmdir(tmp)
    for t in js.get('textures', []):
        e = t.get('extensions', {}).pop('EXT_texture_webp', None)
        if e is not None: t['source'] = e['source']
        if not t.get('extensions'): t.pop('extensions', None)
    js['bufferViews'] = newviews
    js['buffers'] = [{'byteLength': len(out)}]
    for k in ('extensionsUsed', 'extensionsRequired'):
        if k in js:
            js[k] = [x for x in js[k] if x not in ('EXT_meshopt_compression', 'KHR_mesh_quantization', 'EXT_texture_webp')]
            if not js[k]: del js[k]
    jb = json.dumps(js, separators=(',', ':')).encode()
    while len(jb) % 4: jb += b' '
    while len(out) % 4: out.append(0)
    glb = struct.pack('<III', 0x46546C67, 2, 12 + 8 + len(jb) + 8 + len(out)) + struct.pack('<II', len(jb), 0x4E4F534A) + jb + struct.pack('<II', len(out), 0x004E4942) + bytes(out)
    open(outp, 'wb').write(glb)
    # sanity: index ranges
    for m in js['meshes']:
        for pr in m['primitives']:
            pa = js['accessors'][pr['attributes']['POSITION']]
            print(m.get('name'), 'verts', pa['count'], 'min', [round(x, 3) for x in pa['min']], 'max', [round(x, 3) for x in pa['max']])

if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
