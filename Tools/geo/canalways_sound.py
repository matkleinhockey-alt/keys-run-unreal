"""canalways_sound.py: sound every line the bake added to the canals files, in the game.

  python canalways_sound.py script [i n]      -> a script for shoot.sh: the boat is put down every STEP metres along the
                                                 centre of each line that is not in main's file, at dead low tide, and the
                                                 game logs the water under her (fish:sound, a KRSound line a second)
  python canalways_sound.py read <log> [...]  -> the least water found on each line, and every spot under LIMIT

The lines are told from main's by `git show main:<file>`, so run it from the lane's worktree."""
import math, os, re, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
GEO = os.path.normpath(os.path.join(HERE, '..', '..', 'Content', 'KeysRun', 'Geo'))
STEP, DWELL, LIMIT = 12.0, 1.5, 1.0


def new_lines():
    out = []
    for ph in ('p2', 'p3', 'p4'):
        rel = f'Content/KeysRun/Geo/{ph}_canals.txt'
        old = set(subprocess.run(['git', 'show', 'main:' + rel], capture_output=True, text=True, cwd=os.path.join(HERE, '..', '..')).stdout.splitlines())
        for ln in open(f'{GEO}/{ph}_canals.txt').read().splitlines():
            if ln in old or ln.startswith('#'):
                continue
            q = ln.split()
            w = q[0] == 'w'
            v = [float(x) for x in q[(2 if w else 1):]]
            out.append((ph, 'way' if w else 'canal line', list(zip(v[::2], v[1::2]))))
    return out


def points():
    pts = []
    for n, (ph, kind, ln) in enumerate(new_lines()):
        total = sum(math.dist(a, b) for a, b in zip(ln, ln[1:]))
        k = max(1, int(round(total / STEP)))
        for q in range(k + 1):
            s, acc = total * q / k, 0.0
            for a, b in zip(ln, ln[1:]):
                d = math.dist(a, b)
                if acc + d >= s - 1e-9 and d > 0:
                    t = (s - acc) / d
                    pts.append((n, a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t))
                    break
                acc += d
    return pts


if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] == 'script':
        steps = ['3:play', '5:tide=-1', '6:fish:sound']
        t = 8.0
        P = points()
        if len(sys.argv) > 3:                              # script <part> <parts>: a long run in pieces (the game lock lasts 20 minutes)
            i, k = int(sys.argv[2]), int(sys.argv[3])
            P = P[len(P) * i // k:len(P) * (i + 1) // k]
        for n, x, z in P:
            steps.append('%g:tp=%.1f,%.1f,0' % (round(t, 1), x, z))
            t += DWELL
        steps.append('%g:quit' % (t + 2))
        print(';'.join(steps))
    elif len(sys.argv) > 2 and sys.argv[1] == 'read':            # read <log> [<log> ...]
        L = new_lines()
        P = points()
        best = {}
        pat = re.compile(r'KRSound (-?\d+),(-?\d+) .*?: ([\d.]+) m under her \(tide ([-+\d.]+)\).*?: (\w+),')
        n_lines = 0
        for ln in (l for f in sys.argv[2:] for l in open(f, errors='ignore')):
            m = pat.search(ln)
            if not m:
                continue
            n_lines += 1
            x, z, d = float(m[1]), float(m[2]), float(m[3])
            q = min(range(len(P)), key=lambda i: (P[i][1] - x) ** 2 + (P[i][2] - z) ** 2)
            if math.hypot(P[q][1] - x, P[q][2] - z) > 2.5:
                continue                                    # (not at one of the spots: the start, or a line cut short)
            if q not in best or d < best[q]:
                best[q] = d
        per = {}
        for q, d in best.items():
            per.setdefault(P[q][0], []).append((d, P[q][1], P[q][2]))
        low = 0
        for n, (ph, kind, ln) in enumerate(L):
            got = per.get(n, [])
            want = sum(1 for p in P if p[0] == n)
            if not got:
                print('%s %s from %.0f, %.0f: NOT SOUNDED' % (ph, kind, ln[0][0], ln[0][1]))
                low += 1
                continue
            d, x, z = min(got)
            print('%s %-10s %.0f, %.0f to %.0f, %.0f: %d of %d spots sounded, least water %.2f m at dead low tide (at %.0f, %.0f)%s'
                  % (ph, kind, ln[0][0], ln[0][1], ln[-1][0], ln[-1][1], len(got), want, d, x, z, '  UNDER %.1f m' % LIMIT if d < LIMIT else ''))
            low += 1 if d < LIMIT or len(got) < want else 0
        print('%d lines, %d spots, %d sounded; %d KRSound lines; %d lines short of water or not fully sounded' % (len(L), len(P), len(best), n_lines, low))
        sys.exit(1 if low else 0)
    else:
        print(__doc__)
