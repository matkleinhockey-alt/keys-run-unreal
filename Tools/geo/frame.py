"""The game frame for the real-geography map.

Real positions (WGS84 lon/lat) are projected on a local plane about the Vaca Key anchor the HUD has always used,
turned so the island chain runs along game +x (east up the Keys), and compressed SCALE : 1 so runs take minutes.
Game x east-north-east along the chain, game z south-south-east toward the reef (north = -z), metres.
Depths and heights are NOT compressed.
"""
import math
LAT0, LON0 = 24.7130, -81.0900     # Vaca Key anchor = game (-120, 0)
ANCHOR_X, ANCHOR_Z = -120.0, 0.0
M_LAT = 111320.0
M_LON = 111320.0 * math.cos(math.radians(LAT0))
SCALE = 3.0                        # real metres per game metre
THETA = math.radians(19.0)         # the chain's bearing above due east (fitted to US-1, Bahia Honda to Duck Key)

def to_game(lon, lat):
    e = (lon - LON0) * M_LON; n = (lat - LAT0) * M_LAT
    c, s = math.cos(THETA), math.sin(THETA)
    return ANCHOR_X + (e * c + n * s) / SCALE, ANCHOR_Z - (-e * s + n * c) / SCALE

def to_lonlat(x, z):
    a = (x - ANCHOR_X) * SCALE; b = -(z - ANCHOR_Z) * SCALE
    c, s = math.cos(THETA), math.sin(THETA)
    e = a * c - b * s; n = a * s + b * c
    return LON0 + e / M_LON, LAT0 + n / M_LAT
