"""
Fetch real device cycle-history and inject the rate[] array as SCENARIO
into a new timestamped simulation script saved to tests/.

Usage:
    python docs/inject_scenario.py --host 192.168.10.2 --password mypass
    python docs/inject_scenario.py --host 192.168.10.2 --password mypass --save snapshot.json
    python docs/inject_scenario.py --load snapshot.json
"""

import sys, argparse, json, textwrap
from datetime import datetime
from pathlib import Path

# Make docs/ importable
DOCS_DIR  = Path(__file__).resolve().parent
TESTS_DIR = DOCS_DIR.parent / "tests"
sys.path.insert(0, str(DOCS_DIR))

import pprint
import sim1
import fetch_sim   # reuse HTTP helpers + build_sim_input + backtrack_initial_ema


# ── Scenario array formatter ─────────────────────────────────────────

def _fmt_scenario(rates, cols=10):
    """Format a flat list as a multi-line Python list literal."""
    if not rates:
        return "[]"
    lines = []
    for i in range(0, len(rates), cols):
        chunk = rates[i:i + cols]
        lines.append("    " + ", ".join(f"{v:.2f}" for v in chunk) + ",")
    return "[\n" + "\n".join(lines) + "\n]"


# ── Generated script template ─────────────────────────────────────────

_TEMPLATE = '''\
"""
EMA + Alarm score simulation — SCENARIO injected from device.

Fetched : {fetched_at}
Source  : {source}
Records : {record_count}

Run:
    python tests/{filename}
"""

import sys
from pathlib import Path

# Locate docs/ relative to this file so sim1 is importable regardless of cwd
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "docs"))
import sim1

# ═══════════════════════════════════════════════════════════════════
#  CONFIGURATION — copied from device at fetch time
# ═══════════════════════════════════════════════════════════════════

INITIAL_EMA  = {initial_ema:.4f}   # back-calculated EMA before oldest record
ALPHA        = {alpha}
CLAMP        = {clamp}

ZONE_GREEN   = {zone_green}
ZONE_YELLOW  = {zone_yellow}

P1 = {p1}
P2 = {p2}

# ═══════════════════════════════════════════════════════════════════
#  SCENARIO — fetched from device (oldest → newest)
# ═══════════════════════════════════════════════════════════════════

SCENARIO = {scenario}

FLOW_RATES = SCENARIO

# ═══════════════════════════════════════════════════════════════════
#  DATA INSPECTION — raw device snapshot
# ═══════════════════════════════════════════════════════════════════

_RAW = {raw_json}


def print_raw():
    """Print the raw device snapshot for inspection."""
    SEP = "─" * 76
    print()
    print(SEP)
    print("  RAW DEVICE SNAPSHOT")
    print(SEP)
    print(f"  Fetched at     : {{_RAW['fetched_at']}}")
    print(f"  Source         : {{_RAW['source']}}")
    print(f"  EMA on device  : {{_RAW['ema_rate_now']:.2f}} ml/h")
    print(f"  EMA₀ back-calc : {{_RAW['initial_ema']:.4f}} ml/h  "
          f"(EMA before oldest buffered record)")
    print(f"  Records        : {{_RAW['record_count']}}")
    print()
    print(f"  {{' #':>4}}  {{' timestamp':^20}}  {{'rate_ml_h':>10}}"
          f"  {{'interval_s':>10}}  {{'alert':>5}}  {{'cycle':>5}}")
    print(f"  {{'-'*62}}")
    for i, rec in enumerate(_RAW['cycles']):
        print(
            f"  {{i:>4}}  {{str(rec.get('ts','?')):^20}}"
            f"  {{rec.get('rate_ml_h', 0):>10.2f}}"
            f"  {{rec.get('interval_s', 0):>10.1f}}"
            f"  {{rec.get('alert', 0):>5}}"
            f"  {{rec.get('cycle', 0):>5}}"
        )
    print(SEP)
    print()


# ═══════════════════════════════════════════════════════════════════

if __name__ == "__main__":
    # 1. Show raw data
    print_raw()

    # 2. Patch sim1 zone globals so zone() colouring matches device settings
    sim1.ZONE_GREEN  = ZONE_GREEN
    sim1.ZONE_YELLOW = ZONE_YELLOW

    # 3. Run simulation
    rows = sim1.simulate(FLOW_RATES, INITIAL_EMA, ALPHA, CLAMP, P1, P2)
    sim1.print_table(rows, INITIAL_EMA, ALPHA, CLAMP, P1, P2)
'''


# ── Generate sim file ─────────────────────────────────────────────────

def generate_sim(rates, cfg, raw_alg, raw_history, source):
    TESTS_DIR.mkdir(exist_ok=True)
    for old in TESTS_DIR.glob("sim_*.py"):
        old.unlink()
    ts        = datetime.now().strftime("%Y%m%d_%H%M%S")
    filename  = f"sim_{ts}.py"
    out_path  = TESTS_DIR / filename

    # Build raw snapshot (newest-first cycles from API → keep as-is for inspection)
    raw_snap = {
        "fetched_at"   : datetime.now().isoformat(timespec='seconds'),
        "source"       : source,
        "ema_rate_now" : cfg['ema_rate_now'],
        "initial_ema"  : cfg['initial_ema'],
        "record_count" : cfg['record_count'],
        "alg"          : raw_alg,
        "cycles"       : raw_history.get('cycles', []),
    }

    content = _TEMPLATE.format(
        fetched_at   = raw_snap['fetched_at'],
        source       = source,
        record_count = cfg['record_count'],
        filename     = filename,
        initial_ema  = cfg['initial_ema'],
        alpha        = cfg['alpha'],
        clamp        = cfg['clamp'],
        zone_green   = cfg['zone_green'],
        zone_yellow  = cfg['zone_yellow'],
        p1           = cfg['p1'],
        p2           = cfg['p2'],
        scenario     = _fmt_scenario(rates),
        raw_json     = pprint.pformat(raw_snap, indent=4, width=100),
    )

    out_path.write_text(content, encoding='utf-8')
    return out_path


# ── CLI ───────────────────────────────────────────────────────────────

def main():
    ap = argparse.ArgumentParser(
        description='Fetch device data and inject as SCENARIO into a new sim script.'
    )
    src = ap.add_mutually_exclusive_group(required=True)
    src.add_argument('--host',    metavar='IP',   help='Device IP/hostname')
    src.add_argument('--load',    metavar='FILE', help='Load previously saved JSON snapshot')
    ap.add_argument('--password', metavar='PWD',  help='Admin password (required with --host)')
    ap.add_argument('--save',     metavar='FILE', help='Also save raw JSON to file')
    args = ap.parse_args()

    if args.host:
        if not args.password:
            ap.error('--password required when using --host')
        print(f"Connecting to http://{args.host} ...")
        alg, history = fetch_sim.fetch_from_device(args.host, args.password)
        source = f"http://{args.host}"
        raw = {'alg': alg, 'history': history}
        if args.save:
            with open(args.save, 'w') as f:
                json.dump(raw, f, indent=2)
            print(f"Snapshot saved → {args.save}")
    else:
        print(f"Loading {args.load} ...")
        with open(args.load) as f:
            raw = json.load(f)
        alg, history = raw['alg'], raw['history']
        source = args.load

    rates, cfg = fetch_sim.build_sim_input(alg, history)

    out_path = generate_sim(rates, cfg, alg, history, source)

    print(f"\nGenerated → {out_path}")
    print(f"Run with:   python {out_path.relative_to(Path.cwd())}")
    print()

    # Also immediately run the simulation as a preview
    sim1.ZONE_GREEN  = cfg['zone_green']
    sim1.ZONE_YELLOW = cfg['zone_yellow']
    print(f"  Records replayed : {cfg['record_count']}  (ring buffer, oldest → newest)")
    print(f"  EMA on device now: {cfg['ema_rate_now']:.2f} ml/h")
    print(f"  EMA₀ back-calc'd : {cfg['initial_ema']:.2f} ml/h")
    rows = sim1.simulate(rates, cfg['initial_ema'], cfg['alpha'], cfg['clamp'],
                         cfg['p1'], cfg['p2'])
    sim1.print_table(rows, cfg['initial_ema'], cfg['alpha'], cfg['clamp'],
                     cfg['p1'], cfg['p2'])


if __name__ == '__main__':
    main()
