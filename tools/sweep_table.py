import json, glob, re, sys

rows = []
for f in sorted(glob.glob('/tmp/analysis_s1_*.json'),
                key=lambda p: int(re.search(r'_(\d+)\.json$', p).group(1))):
    load = int(re.search(r'_(\d+)\.json$', f).group(1))
    try:
        d = json.load(open(f))
    except Exception as e:
        print('%-4s PARSE FAILED: %s' % (load, e))
        continue
    t = d['per_task'][0]
    rows.append((load, t))

if not rows:
    print('no analysis_s1_*.json found')
    sys.exit(1)

print('%-6s %-10s %-10s %-10s %-10s %-10s' %
      ('load%', 'p50', 'p95', 'p99', 'max', 'miss'))
print('-' * 52)
for load, t in rows:
    g = lambda k: t.get(k, 0.0)
    print('%-6s %-10.3f %-10.3f %-10.3f %-10.3f %-10.4f' %
          (load, g('p50'), g('p95'), g('p99'), g('max'), g('miss_ratio')))

base = rows[0][1]
hi = rows[-1][1]
p99s = [t.get('p99', 0.0) for _, t in rows if t.get('p99')]
print()
if len(p99s) >= 2 and p99s[0] > 0:
    print('BRAKE_CTL p99 at %d%% : %.3f ms' % (rows[0][0], p99s[0]))
    print('BRAKE_CTL p99 at %d%% : %.3f ms' % (rows[-1][0], p99s[-1]))
    growth = (p99s[-1] / p99s[0] - 1) * 100
    print('latency growth        : %+.1f%%' % growth)
    if abs(growth) < 1.0:
        print()
        print('NOTE: p99 is flat across the sweep. Either the platform absorbs')
        print('      this contention, or the metric is not observing it. Do not')
        print('      present a flat curve as a latency-knee result.')
print('total deadline misses : %d' % sum(t.get('misses', 0) for _, t in rows))
print('activations per point : %s' % ', '.join(
    '%d%%=%d' % (load, t.get('activations', 0)) for load, t in rows))