"""Read a baked .kgeo (see bake.py) back into numpy, for site surveys and checks."""
import struct
import numpy as np


class KGeo:
    def __init__(self, path):
        b = open(path, 'rb').read()
        p = 0

        def get(fmt):
            nonlocal p
            v = struct.unpack_from('<' + fmt, b, p)
            p += struct.calcsize('<' + fmt)
            return v if len(v) > 1 else v[0]

        magic, ver = get('Ii')
        assert magic == 0x4F45474B and ver == 2
        self.x0, self.z0, self.cell = get('ddd')
        self.nx, self.nz = get('ii')
        self.scale, self.theta, self.lat0, self.lon0, self.ancx, self.ancz = get('dddddd')
        nchain = get('i')
        self.chain_x0, self.chain_step = get('dd')
        n = self.nx * self.nz
        self.height = np.frombuffer(b, '<i2', n, p).reshape(self.nz, self.nx) / 100.0; p += n * 2
        self.shore = np.frombuffer(b, '<i2', n, p).reshape(self.nz, self.nx) / 10.0; p += n * 2
        self.chain = np.frombuffer(b, '<f4', nchain, p); p += nchain * 4
        self.island = np.frombuffer(b, 'u1', n, p).reshape(self.nz, self.nx); p += n
        self.islands = []
        for _ in range(get('i')):
            ln = get('i')
            name = b[p:p + ln].decode('utf8'); p += ln
            x, z, area, minx, minz, maxx, maxz = get('ddddddd')
            nr = get('i')
            ring = np.frombuffer(b, '<f4', nr, p).reshape(-1, 2); p += nr * 4
            self.islands.append(dict(name=name, x=x, z=z, area=area, bounds=(minx, minz, maxx, maxz), ring=ring))

    def _bil(self, g, x, z):
        fx = np.clip((np.asarray(x, float) - self.x0) / self.cell, 0, self.nx - 1.001)
        fz = np.clip((np.asarray(z, float) - self.z0) / self.cell, 0, self.nz - 1.001)
        i = fx.astype(int); j = fz.astype(int); u = fx - i; v = fz - j
        return (g[j, i] * (1 - u) + g[j, i + 1] * u) * (1 - v) + (g[j + 1, i] * (1 - u) + g[j + 1, i + 1] * u) * v

    def H(self, x, z): return self._bil(self.height, x, z)
    def S(self, x, z): return self._bil(self.shore, x, z)

    def chain_z(self, x):
        f = np.clip((np.asarray(x, float) - self.chain_x0) / self.chain_step, 0, len(self.chain) - 1.001)
        i = f.astype(int)
        return self.chain[i] * (1 - (f - i)) + self.chain[i + 1] * (f - i)
