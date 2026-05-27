"""
Fetch real device data and replay through sim1.py EMA/alarm simulation.

Usage:
    python docs/fetch_sim.py --host 192.168.1.100 --password mypass
    python docs/fetch_sim.py --host 192.168.1.100 --password mypass --save data.json
    python docs/fetch_sim.py --load data.json
"""

import sys, argparse, json
import http.cookiejar, urllib.request, urllib.parse
from pathlib import Path

# Make sim1 importable from docs/
sys.path.insert(0, str(Path(__file__).parent))
import sim1


# ── HTTP helpers ─────────────────────────────────────────────────────

def make_opener(jar):
    return urllib.request.build_opener(urllib.request.HTTPCookieProcessor(jar))

def _read(response):
    return json.loads(response.read())

def _http_call(opener, req):
    try:
        return _read(opener.open(req, timeout=10))
    except urllib.error.HTTPError as e:
        body = e.read().decode(errors='replace')
        try:
            detail = json.loads(body).get('error', body)
        except Exception:
            detail = body[:200]
        raise RuntimeError(f"HTTP {e.code} {e.reason}: {detail}") from None

def post(opener, url, fields):
    data = urllib.parse.urlencode(fields).encode()
    req  = urllib.request.Request(url, data=data, method='POST')
    return _http_call(opener, req)

def get(opener, url):
    return _http_call(opener, urllib.request.Request(url))


# ── Device fetch ─────────────────────────────────────────────────────

def fetch_from_device(host, password):
    jar    = http.cookiejar.CookieJar()
    opener = make_opener(jar)

    resp = post(opener, f'http://{host}/api/login', {'password': password})
    if not resp.get('success'):
        raise RuntimeError(f"Login failed: {resp}")

    alg     = get(opener, f'http://{host}/api/alg-config')
    history = get(opener, f'http://{host}/api/cycle-history')
    return alg, history


# ── Back-calculate starting EMA ───────────────────────────────────────

def backtrack_initial_ema(rates_oldest_first, ema_now, alpha, clamp):
    """
    Reverse the EMA update formula through all records to find the EMA
    value that existed BEFORE the first (oldest) record in the buffer.

    Forward:  ema_new = (1-α)·ema_old + α·clamped
    Backward: ema_old = (ema_new - α·clamped) / (1-α)

    Clamped value depends on ema_old — resolved per-case:
      - no clamp:    clamped = rate         → ema_old = (ema_new - α·rate)/(1-α)
      - clamp high:  clamped = ema_old·(1+c) → ema_old = ema_new/(1 + α·c)
      - clamp low:   clamped = ema_old·(1-c) → ema_old = ema_new/(1 - α·c)
    """
    ema = ema_now
    for rate in reversed(rates_oldest_first):
        if rate == 0.0:
            # Firmware skips EMA update when intervalS==0 (first cycle after reset)
            continue
        # Try no-clamp case first
        ema_old = (ema - alpha * rate) / (1.0 - alpha)
        if ema_old > 0 and ema_old * (1 - clamp) <= rate <= ema_old * (1 + clamp):
            ema = ema_old
            continue
        # High clamp
        ema_old_hi = ema / (1.0 + alpha * clamp)
        if ema_old_hi > 0 and rate > ema_old_hi * (1 + clamp):
            ema = ema_old_hi
            continue
        # Low clamp
        ema_old_lo = ema / (1.0 - alpha * clamp)
        if ema_old_lo > 0 and rate < ema_old_lo * (1 - clamp):
            ema = ema_old_lo
            continue
        # Fallback: no-clamp result (shouldn't happen with valid data)
        ema = max(0.01, ema_old)
    return ema


# ── Build sim input from API responses ───────────────────────────────

def build_sim_input(alg, history):
    cycles = history.get('cycles', [])
    if not cycles:
        raise ValueError("No cycle records in history")

    # API returns newest-first; simulation needs oldest-first
    rates = [c['rate_ml_h'] for c in reversed(cycles)]

    alpha = alg.get('ema_alpha', sim1.ALPHA)
    clamp = alg.get('ema_clamp', sim1.CLAMP)
    ema_now = history.get('ema_rate', alg.get('initial_ema', sim1.INITIAL_EMA))

    # Back-calculate the EMA value before the oldest buffered record
    derived_ema0 = backtrack_initial_ema(rates, ema_now, alpha, clamp)

    cfg = {
        'initial_ema' : derived_ema0,
        'alpha'       : alpha,
        'clamp'       : clamp,
        'p1'          : alg.get('alarm_p1',   sim1.P1),
        'p2'          : alg.get('alarm_p2',   sim1.P2),
        'zone_green'  : alg.get('zone_green', sim1.ZONE_GREEN),
        'zone_yellow' : alg.get('zone_yellow', sim1.ZONE_YELLOW),
        'ema_rate_now': ema_now,
        'record_count': len(cycles),
    }
    return rates, cfg


# ── Run simulation ────────────────────────────────────────────────────

def run(rates, cfg):
    # Patch sim1 zone globals so zone() colouring matches device settings
    sim1.ZONE_GREEN  = cfg['zone_green']
    sim1.ZONE_YELLOW = cfg['zone_yellow']

    print(f"\n  Records replayed : {cfg['record_count']}  (ring buffer, oldest → newest)")
    print(f"  EMA on device now: {cfg['ema_rate_now']:.2f} ml/h")
    print(f"  EMA₀ back-calc'd : {cfg['initial_ema']:.2f} ml/h  "
          f"(EMA before oldest buffered record)")

    rows = sim1.simulate(
        rates,
        cfg['initial_ema'],
        cfg['alpha'],
        cfg['clamp'],
        cfg['p1'],
        cfg['p2'],
    )
    sim1.print_table(rows, cfg['initial_ema'], cfg['alpha'], cfg['clamp'],
                     cfg['p1'], cfg['p2'])


# ── CLI ───────────────────────────────────────────────────────────────

def main():
    ap = argparse.ArgumentParser(description='Replay real device data through sim1.py')
    src = ap.add_mutually_exclusive_group(required=True)
    src.add_argument('--host',     metavar='IP',   help='Device IP/hostname')
    src.add_argument('--load',     metavar='FILE', help='Load previously saved JSON')
    ap.add_argument('--password',  metavar='PWD',  help='Admin password (required with --host)')
    ap.add_argument('--save',      metavar='FILE', help='Save raw JSON to file')
    args = ap.parse_args()

    if args.host:
        if not args.password:
            ap.error('--password required when using --host')
        print(f"Connecting to http://{args.host} ...")
        alg, history = fetch_from_device(args.host, args.password)
        raw = {'alg': alg, 'history': history}
        if args.save:
            with open(args.save, 'w') as f:
                json.dump(raw, f, indent=2)
            print(f"Saved to {args.save}")
    else:
        with open(args.load) as f:
            raw = json.load(f)
        alg, history = raw['alg'], raw['history']

    rates, cfg = build_sim_input(alg, history)
    run(rates, cfg)


if __name__ == '__main__':
    main()
