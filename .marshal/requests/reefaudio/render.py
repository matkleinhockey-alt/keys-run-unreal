#!/usr/bin/env python3
"""Offline copy of the reef audio's synth maths (lane reefaudio, criterion 5).

    render.py <recipes.json> <out dir> [<recording.wav> ...]

recipes.json is written by the game itself (script step `con=kr.reefaudio dump`): every sound's notes exactly as
the game hands them to its synth, and the bed's settings for eight places. This file holds a copy of the audio
thread's arithmetic (Source/KeysRun/KeysReefAudioSynth.cpp: the note voice, the shrimp clicks, the three noise
layers) and, for the comparison, of the hub's own (KeysSfxSynth.cpp: the engine voice, the water and wind loops,
the one-shot burst, the compressor; KeysAudio.cpp: the engine's settings and the old under-water bed). It writes
one picture a sound (waveform over spectrogram), a contact sheet, and levels.md.

What is NOT the same as the game: the noise table is drawn with numpy's generator, not Unreal's FRandomStream
(same recipe, different numbers); the engine and the loops are rendered at their settled values (the game glides
to them); the hub's under-water low-pass is taken to be a one-pole filter (Unreal's source low-pass), which this
lane has not confirmed; and sound that the game makes on two synth components is added here as plain numbers.
Any WAV given is a recording of the game's own reef synth (`con=kr.reefaudio rec <s> <name>`) and gets the same
picture, so the two can be laid side by side.
"""
import json, math, sys, os, wave
import numpy as np
from scipy.signal import lfilter, spectrogram
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

SR = 48000
BLOCK = 32
rng = np.random.default_rng(0x6b657973)

# ---------------------------------------------------------------- KeysSynth.cpp
def make_noise_table():
    w = rng.random(SR * 2) * 2 - 1
    b = lfilter([.03], [1, -.97], w)          # b = .97 b + .03 w
    return (w * .6 + b * 2.5).astype(np.float64)
NOISE = make_noise_table()

def engine_table():
    rough = .8
    H = [(1, .32), (2, .75 * (.6 + rough)), (3, .16), (4, 1.), (5, .08), (6, .32), (7, .05), (8, .46), (10, .14), (12, .26),
         (14, .07), (16, .18), (20, .09), (24, .07), (32, .04)]
    ph = 2 * np.pi * np.arange(2048) / 2048
    s = sum(a * .45 * math.sin(k * 1.7) * np.cos(k * ph) + a * np.sin(k * ph) for k, a in H)
    return s / np.abs(s).max()
ENGINE = engine_table()

class Noise:
    def __init__(self): self.pos = int(rng.integers(0, len(NOISE)))
    def take(self, n):
        idx = (self.pos + np.arange(n)) % len(NOISE)
        self.pos = (self.pos + n) % len(NOISE)
        return NOISE[idx]

def polyblep(t, dt):
    out = np.zeros_like(t)
    if dt <= 0: return out
    a = t < dt; x = t[a] / dt; out[a] = x + x - x * x - 1
    b = t > 1 - dt; x = (t[b] - 1) / dt; out[b] = x * x + x + x + 1
    return out

class Osc:
    """FOsc: wave 0 sine, 1 square, 2 saw, 3 triangle, 4 the engine table."""
    def __init__(self, wave=0, phase=0.): self.wave = wave; self.phase = phase
    def take(self, inc, n):
        inc = min(max(inc, 0.), .49)
        ph = (self.phase + inc * np.arange(n)) % 1.0
        self.phase = (self.phase + inc * n) % 1.0
        if self.wave == 0: return np.sin(2 * np.pi * ph)
        if self.wave == 4: return np.interp(ph * 2048, np.arange(2049), np.append(ENGINE, ENGINE[0]))
        if self.wave == 2: return 2 * ph - 1 - polyblep(ph, inc)
        if self.wave == 1: return np.where(ph < .5, 1., -1.) + polyblep(ph, inc) - polyblep((ph + .5) % 1.0, inc)
        return np.where(ph < .5, 4 * ph - 1, 3 - 4 * ph)

class Biquad:
    """FBiquad: 0 lowpass, 1 highpass (Q in dB), 2 bandpass (plain Q); transposed direct form II like lfilter."""
    def __init__(self): self.z = np.zeros(2); self.b = np.array([1., 0, 0]); self.a = np.array([1., 0, 0])
    def set(self, typ, f, q):
        f = min(max(f, 10.), SR * .45)
        w0 = 2 * np.pi * f / SR; cs, sn = math.cos(w0), math.sin(w0)
        if typ == 0: al = sn / (2 * 10 ** (q / 20)); b = [(1 - cs) * .5, 1 - cs, (1 - cs) * .5]
        elif typ == 1: al = sn / (2 * 10 ** (q / 20)); b = [(1 + cs) * .5, -(1 + cs), (1 + cs) * .5]
        else: al = sn / (2 * max(q, 1e-4)); b = [al, 0, -al]
        a0 = 1 + al
        self.b = np.array(b) / a0; self.a = np.array([1, -2 * cs / a0, (1 - al) / a0])
    def run(self, x):
        y, self.z = lfilter(self.b, self.a, x, zi=self.z)
        return y

def exp_ramp(a, b, u):
    u = min(max(u, 0.), 1.)
    if a <= 0 or b <= 0: return a + (b - a) * u
    return a * (b / a) ** u

def one_pole(x, hz, z=0.):
    k = 1 - math.exp(-2 * np.pi * hz / SR)
    y, _ = lfilter([k], [1, -(1 - k)], x, zi=[z * (1 - k)])
    return y

# ---------------------------------------------------------------- KeysReefAudioSynth.cpp: one note
def render_note(n, out):
    """UKeysReefSynth::RenderNote, block by block; out is (frames, 2)."""
    dur = max(n["dur"], .004); att = min(max(n["att"], .0003), dur * .9); rel = min(max(n["rel"], 0.), dur - att)
    gain = n["gain"]
    def env(t):
        if t <= 0 or t >= dur: return 0.
        if t < att: return gain * t / att
        if rel > 0: return gain if t < dur - rel else gain * (dur - t) / rel
        return exp_ramp(gain, .0001, (t - att) / max(dur - att, .001))
    delay_s = int(n["delay"] * SR)
    osc = Osc(n["wave"]); noise = Noise(); flt = Biquad(); vib_ph = 0.; am_ph = 0.; lpz = 0.
    filt = (not n["tone"]) or n["fc0"] > 0
    lpk = 1 - math.exp(-2 * np.pi * n["lp"] / SR) if n["lp"] > 0 else 1.
    age = 0
    total = len(out)
    while age < total:
        frames = min(BLOCK, total - age)
        start = age; age += frames
        if age <= delay_s: continue
        skip = max(0, delay_s - start)
        t0 = (start + skip - delay_s) / SR; t1 = (age - delay_s) / SR
        if t0 >= dur: break
        e0, e1 = env(t0), env(t1); count = frames - skip
        u = t0 / dur
        if n["tone"]:
            if n["f1"] > 0: hz = exp_ramp(n["f0"], n["f1"], u * 2) if u < .5 else exp_ramp(n["f1"], n["f2"] if n["f2"] > 0 else n["f1"], (u - .5) * 2)
            else: hz = exp_ramp(n["f0"], n["f2"], u) if n["f2"] > 0 else n["f0"]
            if n["vib"] > 0:
                hz *= 1 + n["vib"] * math.sin(2 * np.pi * vib_ph); vib_ph = (vib_ph + n["vibhz"] * frames / SR) % 1.0
            s = osc.take(hz / SR, count)
        else:
            s = noise.take(count)
        if filt:
            fc0 = n["fc0"] if n["fc0"] > 0 else 1000.
            flt.set(n["filter"], exp_ramp(fc0, n["fc1"], u) if n["fc1"] > 0 else fc0, n["q"])
            s = flt.run(s)
        if n["am"] > 0:
            ph = (am_ph + n["amhz"] / SR * np.arange(count)) % 1.0
            am_ph = (am_ph + n["amhz"] / SR * count) % 1.0
            s = s * (1 - n["am"] * (.5 + .5 * np.sin(2 * np.pi * ph)))
        s = s * (e0 + (e1 - e0) / count * np.arange(count))
        if lpk < 1:
            y, zf = lfilter([lpk], [1, -(1 - lpk)], s, zi=[lpz * (1 - lpk)]); lpz = y[-1]; s = y
        out[start + skip:age, 0] += s * n["l"]; out[start + skip:age, 1] += s * n["r"]

def render_sound(snd, tail=.15):
    length = max(n["delay"] + n["dur"] for n in snd["notes"]) + tail
    out = np.zeros((int(length * SR), 2))
    for n in snd["notes"]: render_note(n, out)
    return out

# ---------------------------------------------------------------- KeysReefAudioSynth.cpp: the bed
def render_bed(b, secs=6.0):
    """UKeysReefSynth::RenderBlock's bed at settled values: clicks, sizzle, surge, deep, the one-pole low-pass."""
    n = int(secs * SR); out = np.zeros((n, 2))
    rate = min(max(b["snaprate"], 0.), 80.)
    t = .05; clicks = 0
    click_len = int(.010 * SR)
    while rate > .01:
        t += -math.log(max(1e-6, rng.random())) / rate
        i = int(t * SR)
        if i + click_len >= n: break
        u = rng.random(); g = b["snapgain"] * (.06 + .94 * u ** 3); pan = rng.random()
        bp = Biquad(); bp.set(2, 2400 + (7800 - 2400) * rng.random() * (.45 + .55 * u), .8 + 1.6 * rng.random())
        tt = np.arange(click_len) / SR
        x = Noise().take(click_len) * 2 * np.exp(-tt / .0012); x[0] += 4
        s = bp.run(x) * g
        out[i:i + click_len, 0] += s * math.cos(pan * np.pi / 2) * 1.41421; out[i:i + click_len, 1] += s * math.sin(pan * np.pi / 2) * 1.41421
        clicks += 1
    f = Biquad(); f.set(2, 5200, .7); mono = f.run(Noise().take(n)) * b["sizzle"]
    f = Biquad(); f.set(0, b["surgehz"], 0); mono += f.run(Noise().take(n)) * b["surge"]
    f = Biquad(); f.set(0, 70, 0); mono += f.run(Noise().take(n)) * b["deep"]
    out += mono[:, None]
    for c in (0, 1): out[:, c] = one_pole(out[:, c], min(max(b["lp"], 200.), 20000.))
    return out, clicks

# ---------------------------------------------------------------- the hub, for comparison (KeysSfxSynth.cpp, KeysAudio.cpp)
def hub_burst(b, n_total):
    """UKeysSfxSynth::RenderBurst (not the gull)."""
    out = np.zeros(n_total)
    dur = max(b.get("dur", .3), .01); att = min(max(b.get("att", .005), .0005), dur * .9); gain = b.get("gain", .4)
    def env(t):
        if t <= 0: return 0.
        if t < att: return gain * t / att
        if t >= dur: return 0.
        return exp_ramp(gain, .0001, (t - att) / max(dur - att, .001))
    delay_s = int(b.get("delay", 0) * SR); osc = Osc(b.get("wave", 0)); noise = Noise(); flt = Biquad(); age = 0
    while age < n_total:
        frames = min(BLOCK, n_total - age); start = age; age += frames
        if age <= delay_s: continue
        skip = max(0, delay_s - start); t0 = (start + skip - delay_s) / SR; t1 = (age - delay_s) / SR
        if t0 > dur + .02: break
        count = frames - skip; e0, e1 = env(t0), env(t1)
        if b.get("tone"):
            hz = exp_ramp(b["f"], b["fto"], t0 / dur) if b.get("fto", 0) > 0 else b["f"]
            s = osc.take(hz / SR, count)
        else:
            flt.set(b.get("filter", 0), exp_ramp(b["f"], b["fto"], t0 / dur) if b.get("fto", 0) > 0 else b["f"], b.get("q", .8))
            s = flt.run(noise.take(count))
        out[start + skip:age] += s * (e0 + (e1 - e0) / count * np.arange(count))
    return out

def comp_curve_db(x):
    thr, knee, ratio = -14., 30., 4.
    if x <= thr: return x
    if x <= thr + knee: d = x - thr; return x + (1 / ratio - 1) * d * d / (2 * knee)
    return thr + knee + (1 / ratio - 1) * knee * .5 + (x - thr - knee) / ratio

def compress(x):
    """FCompressor::Process on a mono signal."""
    makeup = 10 ** (-.6 * comp_curve_db(0.) / 20)
    ak = 1 - math.exp(-1 / SR / .003); rk = 1 - math.exp(-1 / SR / .25)
    env = 0.; y = np.empty_like(x)
    for i, v in enumerate(x):
        p = abs(v); env += (p - env) * (ak if p > env else rk)
        db = 8.68588964 * math.log(max(env, 1e-6))
        y[i] = min(max(v * 10 ** ((comp_curve_db(db) - db) / 20) * makeup, -1.), 1.)
    return y

def hub_boat(thr, f, secs=3.0, engines=2, under_hz=0.):
    """The player's engine (AKeysAudio::UpdateEngine / PushEngine, UKeysSfxSynth::RenderBlock) plus the water and wind
    loops (UpdateLoops) at throttle thr and speed fraction f, settled, in gear, trim .2, calm sea; mono (the left
    channel of a centred listener), after the master gain and the compressor. under_hz > 0: through the hub's
    under-water low-pass at that frequency."""
    n = int(secs * SR); t = np.arange(n) / SR; N = engines
    idle, mx = 700., 6400.
    rpm = idle + (mx - idle) * min(max(thr * .82 + f * .25 + (.2 - .4) * .08 * thr, 0.), 1.2)
    load = min(max(thr - f * .7 + .25, 0.), 1.)
    crank = rpm / 60; r = min(max((rpm - idle) / (mx - idle), 0.), 1.2); r1 = min(r, 1.); lo = (1 - r1) ** 2
    cyl = np.zeros(n)
    for i in range(N):
        base = .6 / math.sqrt(N); off = (i - (N - 1) * .5) * .0009
        fr = crank * (1 + off)
        w = Osc(4, rng.random()).take(fr / SR, n)
        lope = 1 + (.62 * lo + .06) * np.sin(2 * np.pi * crank * .5 * (1 + off) * t)
        pan = 1.0 if N == 1 else math.cos((((i - (N - 1) * .5) / ((N - 1) * .5) * .55) + 1) * .5 * np.pi / 2)
        cyl += w * base * (1 - .62 * r1 * r1) * lope * pan
    mf = Biquad(); mf.set(0, (260 + r * 1250) * 1.25 * (.8 + .35 * load), 1.1)
    rf = Biquad(); rf.set(1, 260, 1.)
    k = 8 + .8 * 40
    hp = np.clip(rf.run(cyl), -1, 1); shaped = (1 + k) * hp / (1 + k * np.abs(hp))
    e = mf.run(cyl) + shaped * (.17 * lo + .09 + .05 * r + .06 * load)
    m = np.sin(2 * np.pi * crank * 2 * t) * (.24 * (1 + .6 * load) * (1 + 1.1 * r1))
    bb = Biquad(); bb.set(2, 170, 1.2); el = Biquad(); el.set(0, 7, 1.)
    m += bb.run(Noise().take(n)) * (.035 * lo * math.sqrt(N) + .5 * lo * el.run(Noise().take(n)))
    ib = Biquad(); ib.set(2, 1400 + r * 1600, 1.1); m += ib.run(Noise().take(n)) * (.015 + .55 * load * r)
    m += np.sin(2 * np.pi * rpm * .21 * t) * (.003 + .01 * r * r)
    wf = rpm / 60 * 27; m += (np.sin(2 * np.pi * wf * t) + np.sin(2 * np.pi * wf * 1.0015 * t)) * (.0075 + .14 * r * r)
    ff = crank * 4; m += (Osc(3).take(ff / SR, n) + Osc(3).take(ff * 1.006 / SR, n)) * ((.04 + .48 * r1 * r1) * (.7 + .3 * load))
    eng = (e + m) * (.07 * 1.08 * (.36 + .45 * r + .3 * load) * (1 + .12 * (N - 1)))
    # the loops (sea factor 1.2 in a 7 m/s breeze, no rain)
    sea = 1.2
    q = Biquad(); q.set(0, 400 + f * 900, .6); amb = q.run(Noise().take(n)) * (.04 + f * .32)
    q = Biquad(); q.set(1, 3200, .5); amb += q.run(Noise().take(n)) * (f * f * .12)
    q = Biquad(); q.set(2, 400 + f * 700, .4); amb += q.run(Noise().take(n)) * (.015 + f * .09 + sea * .015)
    q = Biquad(); q.set(0, 380, .5); amb += q.run(Noise().take(n)) * ((.05 + .05 * sea) * .6)
    parts = {"engine": eng * .8, "water and wind": amb * .8}
    mix = compress((eng + amb) * .8)
    if under_hz > 0: mix = one_pole(mix, under_hz); parts = {k2: one_pole(v, under_hz) for k2, v in parts.items()}
    return mix, parts

def hub_under_bed(depth, secs=6.0, holding=False):
    """The hub's own under-water sounds (AKeysAudio::SetUnderwater): the breathing loop, the mask's bubble pops and
    the old flat crackle (2.2 a second), through the master gain and the hub's low-pass at 900 exp(-depth / 14) Hz."""
    n = int(secs * SR)
    bf = Biquad(); bf.set(2, 520 if holding else 440, .9)
    ph = (np.arange(n) / SR / 4.2) % 1.0
    breath = bf.run(Noise().take(n)) * np.maximum(0, (.1 if holding else .04) + (.07 if holding else .02) * np.sin(2 * np.pi * ph))
    pops = np.zeros(n); t = .3
    while t < secs - .2:
        pops += hub_burst({"filter": 2, "f": rng.uniform(900, 2600), "q": 3, "gain": rng.uniform(.04, .09), "att": .004, "dur": .09, "delay": t}, n)
        t += rng.uniform(.4, 1.4) if holding else rng.uniform(1.5, 4)
    crack = np.zeros(n)
    for _ in range(rng.poisson(2.2 * secs)):
        crack += hub_burst({"filter": 2, "f": rng.uniform(3200, 6500), "q": rng.uniform(4, 9), "gain": rng.uniform(.01, .03), "att": .002, "dur": .05, "delay": rng.uniform(0, secs - .1)}, n)
    hz = max(150., 900 * math.exp(-depth / 14))
    return {k: one_pole(v * .8, hz) for k, v in {"breathing loop": breath, "mask bubbles": pops, "old flat crackle": crack}.items()}, hz

def hub_crunch(depth, dist=2.0):
    """The stoplight parrotfish's bite (KeysReefLifeParrot.cpp) as the hub plays it: 3 bursts at its reference
    distance of 2 m, the master gain, the hub's low-pass."""
    n = int(.3 * SR)
    s = hub_burst({"filter": 2, "f": 3000, "fto": 1700, "q": 2.2, "gain": .3, "att": .002, "dur": .07}, n)
    s += hub_burst({"filter": 2, "f": 2300, "fto": 1300, "q": 2., "gain": .2, "att": .002, "dur": .055, "delay": .05}, n)
    s += hub_burst({"tone": 1, "wave": 3, "f": 480, "fto": 240, "gain": .1, "att": .002, "dur": .035}, n)
    g = 2. / (2. + 1.1 * (max(dist, 2.) - 2.))
    return one_pole(s * .8 * g, max(150., 900 * math.exp(-depth / 14)))

# ---------------------------------------------------------------- measuring and drawing
def db(v): return 20 * math.log10(v) if v > 1e-9 else -180.
def peak(x): return float(np.abs(x).max())
def rms(x): return float(np.sqrt(np.mean(np.square(x))))
def loud_rms(x, win=.05):
    """the loudest 50 ms: what a short sound's level is while it sounds"""
    m = x if x.ndim == 1 else x.max(axis=1) * 0 + np.sqrt(np.mean(np.square(x), axis=1))
    w = max(1, int(win * SR)); c = np.cumsum(np.insert(np.square(m), 0, 0))
    if len(m) <= w: return rms(m)
    return float(np.sqrt(((c[w:] - c[:-w]) / w).max()))

def picture(x, title, path, sub=""):
    mono = x if x.ndim == 1 else x.mean(axis=1)
    t = np.arange(len(mono)) / SR
    fig, ax = plt.subplots(2, 1, figsize=(9, 5), gridspec_kw={"height_ratios": [1, 1.6]})
    ax[0].plot(t, mono, lw=.5, color="#1f4e79"); ax[0].set_xlim(0, t[-1]); ax[0].set_ylabel("sample value")
    pk = max(peak(mono), 1e-6); ax[0].set_ylim(-pk * 1.1, pk * 1.1)
    ax[0].set_title(f"{title}\n{sub}", fontsize=9, loc="left")
    nper = 256 if len(mono) < SR * 3 else 1024
    f, tt, S = spectrogram(mono, SR, nperseg=nper, noverlap=nper * 3 // 4)
    ax[1].pcolormesh(tt, f / 1000, 10 * np.log10(S + 1e-14), vmin=-125, vmax=-45, cmap="magma", shading="auto")
    ax[1].set_ylim(0, 16); ax[1].set_ylabel("kHz"); ax[1].set_xlabel("seconds"); ax[1].set_xlim(0, t[-1])
    fig.tight_layout(); fig.savefig(path, dpi=110); plt.close(fig)

def read_wav(path):
    with wave.open(path) as w:
        a = np.frombuffer(w.readframes(w.getnframes()), dtype="<i2").astype(np.float64) / 32767.
        return a.reshape(-1, w.getnchannels()), w.getframerate()

def main():
    recipes, outdir = sys.argv[1], sys.argv[2]
    os.makedirs(outdir, exist_ok=True)
    J = json.load(open(recipes))
    L = ["# Levels (offline copy of the synth maths; nobody has listened)", "",
         "All figures are dB relative to digital full scale (0 dB = a sample value of 1) at the synths' outputs, before the",
         "system volume. \"Loudest 50 ms\" is the RMS of the loudest twentieth of a second. Produced by `render.py` from",
         "`recipes.json`, which the game wrote.", ""]
    # ---- the references
    idle, _ = hub_boat(0., 0.); cruise, _ = hub_boat(.6, .5); flat, _ = hub_boat(1., 1.)
    ref_idle, ref_cruise, ref_flat = rms(idle[SR // 2:]), rms(cruise[SR // 2:]), rms(flat[SR // 2:])
    L += ["## The references: the hub's own sound", "", "| What | RMS dB | Peak dB |", "|---|---|---|",
          f"| Engines idling in gear, boat still, with the water and wind loops (two engines) | {db(ref_idle):.1f} | {db(peak(idle)):.1f} |",
          f"| Half speed, throttle 0.6 | {db(ref_cruise):.1f} | {db(peak(cruise)):.1f} |",
          f"| Flat out | {db(ref_flat):.1f} | {db(peak(flat)):.1f} |"]
    under = {}
    for d in (3, 12, 25):
        hz = max(150., 900 * math.exp(-d / 14))
        eu, _ = hub_boat(0., 0., under_hz=hz); parts, _ = hub_under_bed(d)
        tot = eu[:len(eu)] ; bed = sum(parts.values())
        under[d] = (rms(eu[SR // 2:]), rms(bed), hz, {k: (rms(v), peak(v)) for k, v in parts.items()})
        L.append(f"| Under water {d} m down, boat idling above (hub low-pass {hz:.0f} Hz): the idling boat | {db(under[d][0]):.1f} | {db(peak(eu)):.1f} |")
        for k, (r, p) in under[d][3].items(): L.append(f"| ... the hub's {k} | {db(r):.1f} | {db(p):.1f} |")
        cr = hub_crunch(d)
        L.append(f"| ... a stoplight parrotfish's crunch 2 m away (loudest 50 ms) | {db(loud_rms(cr)):.1f} | {db(peak(cr)):.1f} |")
        under[d] += (loud_rms(cr), peak(cr))
    L.append("")
    # ---- the bed
    L += ["## The reef's bed, by place", "", "| Place | Clicks a second | RMS dB | Peak dB | Against the hub's whole under-water bed at that depth (dB) | Against the idling boat heard from there (dB) | Against the parrotfish crunch at 2 m, loudest 50 ms (dB) |", "|---|---|---|---|---|---|---|"]
    names = []
    for i, b in enumerate(J["beds"]):
        x, clicks = render_bed(b)
        d = min((3, 12, 25), key=lambda q: abs(q - b["diver"])); u = under[d]
        hub_bed = math.sqrt(sum(r * r for r, _ in u[3].values()))
        r = rms(x.mean(axis=1))
        L.append(f"| {b['name']} | {b['snaprate']:.1f} | {db(r):.1f} | {db(peak(x)):.1f} | {db(r) - db(hub_bed):+.1f} | {db(r) - db(u[0]):+.1f} | {db(r) - db(u[4]):+.1f} |")
        fn = f"bed_{i + 1}.png"; names.append(fn)
        picture(x, f"bed: {b['name']}", os.path.join(outdir, fn),
                f"snaps {b['snaprate']:.1f}/s peak {b['snapgain']:.3f}, sizzle {b['sizzle']:.4f}, surge {b['surge']:.4f} at {b['surgehz']:.0f} Hz, deep {b['deep']:.4f}, low-pass {b['lp']:.0f} Hz; RMS {db(r):.1f} dB, peak {db(peak(x)):.1f} dB")
    L.append("")
    # ---- the sounds
    L += ["## The one-shot sounds, at their reference distance", "",
          "| Sound | Where | Ref m | Never past m | Notes | Peak dB | Loudest 50 ms dB | Against the idling boat (dB) | Against half speed (dB) | Against flat out (dB) | Against the reef bed on the reef top at noon (dB) |", "|---|---|---|---|---|---|---|---|---|---|---|"]
    bed_top, _ = render_bed(J["beds"][0]); bed_top_r = rms(bed_top.mean(axis=1))
    for s in J["sounds"]:
        x = render_sound(s); m = x.mean(axis=1); lr = loud_rms(m)
        reef = bool(s["reef"])
        a_idle = under[3][0] if reef else ref_idle
        row = f"| {s['name']} | {'under water' if reef else 'above water'} | {s['ref']:g} | {s['max']:g} | {len(s['notes'])} | {db(peak(m)):.1f} | {db(lr):.1f} | {db(lr) - db(a_idle):+.1f} | "
        row += ("n/a | n/a | " if reef else f"{db(lr) - db(ref_cruise):+.1f} | {db(lr) - db(ref_flat):+.1f} | ")
        row += (f"{db(lr) - db(bed_top_r):+.1f} |" if reef else "n/a |")
        L.append(row)
        fn = "snd_" + s["name"].replace(".", "_") + ".png"; names.append(fn)
        picture(x, f"{s['name']} at {s['ref']:g} m ({'under water' if reef else 'above water'})", os.path.join(outdir, fn), s["desc"][:150] + f"\npeak {db(peak(m)):.1f} dB, loudest 50 ms {db(lr):.1f} dB")
    L += ["", "For an under-water sound \"the idling boat\" is the boat as heard 3 m down through the hub's low-pass.", ""]
    L += ["## What each sound is made of (the game's own words, seed 1)", ""]
    L += [f"- `{s['name']}`: {s['desc']}." for s in J["sounds"]]
    L.append("")
    # ---- recordings of the game itself
    for wv in sys.argv[3:]:
        x, sr = read_wav(wv)
        base = os.path.splitext(os.path.basename(wv))[0]
        m = x.mean(axis=1)
        L += [f"## Recording of the game's reef synth: {base}.wav", "", f"{len(m) / sr:.1f} s at {sr} Hz; peak {db(peak(x)):.1f} dB; RMS {db(rms(m)):.1f} dB; loudest 50 ms {db(loud_rms(m)):.1f} dB.", ""]
        # second by second, so a run of test sounds can be read off against the table above
        per = [f"{db(peak(m[i:i + sr])):.0f}" for i in range(0, len(m) - sr + 1, sr)]
        L += ["Peak dB of each second: " + " ".join(per), ""]
        seg = 20 * sr
        for k in range(0, len(m), seg):
            fn = f"rec_{base}_{k // seg + 1}.png"; names.append(fn)
            picture(x[k:k + seg], f"recording of the game: {base}.wav from {k / sr:.0f} s", os.path.join(outdir, fn), f"peak {db(peak(x[k:k + seg])):.1f} dB")
    open(os.path.join(outdir, "levels.md"), "w").write("\n".join(L) + "\n")
    print("\n".join(L))
    print("pictures:", len(names))

if __name__ == "__main__":
    main()
