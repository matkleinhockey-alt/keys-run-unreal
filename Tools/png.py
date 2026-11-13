# png.py: a small pure-Python PNG reader / writer (8-bit gray, RGB or RGBA, non-interlaced), for the import tools
import zlib, struct

def read(path):
    b = open(path, 'rb').read()
    assert b[:8] == b'\x89PNG\r\n\x1a\n'
    off = 8; idat = b''; w = h = ct = 0
    while off < len(b):
        ln, ty = struct.unpack('>I4s', b[off:off + 8]); c = b[off + 8:off + 8 + ln]
        if ty == b'IHDR':
            w, h, bd, ct, _, _, il = struct.unpack('>IIBBBBB', c)
            assert bd == 8 and il == 0 and ct in (0, 2, 6), 'unsupported png'
        elif ty == b'IDAT': idat += c
        off += 12 + ln
    n = {0: 1, 2: 3, 6: 4}[ct]
    raw = zlib.decompress(idat); st = w * n
    out = bytearray(h * st); prev = bytearray(st); p = 0
    for y in range(h):
        f = raw[p]; line = bytearray(raw[p + 1:p + 1 + st]); p += 1 + st
        if f == 1:
            for i in range(n, st): line[i] = (line[i] + line[i - n]) & 255
        elif f == 2:
            for i in range(st): line[i] = (line[i] + prev[i]) & 255
        elif f == 3:
            for i in range(st): line[i] = (line[i] + ((line[i - n] if i >= n else 0) + prev[i]) // 2) & 255
        elif f == 4:
            for i in range(st):
                a = line[i - n] if i >= n else 0; c_ = prev[i - n] if i >= n else 0; bb = prev[i]
                pa = abs(bb - c_); pb = abs(a - c_); pc = abs(a + bb - 2 * c_)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else (bb if pb <= pc else c_))) & 255
        out[y * st:(y + 1) * st] = line; prev = line
    return w, h, n, out

def write(path, w, h, n, data):
    ct = {1: 0, 3: 2, 4: 6}[n]; st = w * n
    raw = b''.join(b'\x00' + bytes(data[y * st:(y + 1) * st]) for y in range(h))
    def chunk(t, c): return struct.pack('>I', len(c)) + t + c + struct.pack('>I', zlib.crc32(t + c) & 0xffffffff)
    open(path, 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, ct, 0, 0, 0))
                           + chunk(b'IDAT', zlib.compress(raw, 6)) + chunk(b'IEND', b''))
