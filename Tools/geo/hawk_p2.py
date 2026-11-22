"""hawk_p2.py <p2_hawk_route.json> [speed m/s]: the scripted run down Hawk Channel as a KRScript string.

The route is the one preview_p2.py finds and checks on the data. The game has no autopilot, so the script steers by
the clock: full throttle, a new heading at the time the boat should reach each waypoint (the boat runs at about
41.5 m/s flat out), and `picks` every 20 seconds, which logs where the boat really is and the depth under it."""
import json, math, sys

route = json.load(open(sys.argv[1]))['hawk']
speed = float(sys.argv[2]) if len(sys.argv) > 2 else 41.5
T0 = 6.0
steps = [(3, 'play')]
t = T0
for k, ((ax, az), (bx, bz)) in enumerate(zip(route[:-1], route[1:])):
    h = math.atan2(-(bx - ax), -(bz - az)) % (2 * math.pi)     # forward is (-sin H, -cos H)
    if k == 0:
        steps += [(t, f'tp={ax:.0f},{az:.0f},{h:.3f}'), (t + .5, 'throttle=1')]
        t += 3.0      # getting on plane
    steps.append((t, f'heading={h:.3f}'))
    t += math.hypot(bx - ax, bz - az) / speed
end = t
p = T0 + 4
while p < end:
    steps.append((p, 'picks')); p += 20
steps += [(end, 'picks'), (end + .5, 'shot'), (end + 1.5, 'quit')]
steps.sort(key=lambda s: s[0])
print(';'.join(f'{a:.1f}:{b}' for a, b in steps))
