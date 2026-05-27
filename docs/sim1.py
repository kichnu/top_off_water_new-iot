"""
EMA + Alarm score simulation — single-table output.

CONFIGURE the block below, then run:
    python ema_alarm_sim2.py
"""

import math

# ═══════════════════════════════════════════════════════════════════
#  CONFIGURATION — edit this block
# ═══════════════════════════════════════════════════════════════════

INITIAL_EMA = 20.0   # bootstrapped EMA value (ml/h)
ALPHA       = 0.50    # EMA smoothing: 0.1=slow .. 0.5=fast
CLAMP       = 0.40    # max allowed deviation per step: 0.20 = ±20%

# Alarm score zone thresholds
ZONE_GREEN  = 20.0    # score < GREEN  → green
ZONE_YELLOW = 50.0    # GREEN ≤ score < YELLOW → yellow, else red

# Alarm parameters
P1 = 0.1   # magnitude weight: how strongly a single spike contributes
P2 = 0.80   # decay speed: larger = score drops faster when rate returns to EMA

# ───────────────────────────────────────────────────────────────────
#  FLOW_RATE SEQUENCE — pick one scenario or define your own
# ───────────────────────────────────────────────────────────────────
#
# SCENARIO_A: stable operation (~100 ml/h with small natural variation)
SCENARIO_A = [
    20, 28, 22, 27, 2, 2, 2, 23, 22,
    30, 29,  22, 33, 30, 33, 34, 34, 39,
    30, 29,  22, 33, 30, 33, 34, 140, 100, 190, 100, 100, 500, 300, 390, 100, 100,
    40, 48, 46, 45, 49, 52, 54, 55, 66, 65, 66,
    75, 77, 78, 220,280, 91, 99, 100, 101, 99, 100,
]

# SCENARIO_B: single short failure (pump ran dry for 3 cycles), then back to normal
SCENARIO_B = [
    100, 98, 102, 100, 99, 101, 98, 102, 100, 99,
    100, 98, 102, 100,  # normal
    2000, 2000, 2000,   # failure: sensor stuck, over-dosing
    99, 101, 100, 98, 102, 100, 99, 101, 98, 102,
    100, 98, 102, 100, 99, 101, 98, 102, 100, 99,
    100, 98, 102, 100, 99, 101, 98, 102, 100, 99,
    100, 98,
]

# SCENARIO_C: gradual real increase (summer evaporation season, slow drift)
SCENARIO_C = [
    100, 100, 101, 102, 102, 103, 104, 104, 105, 106,
    107, 108, 108, 109, 110, 111, 112, 113, 114, 115,
    116, 117, 118, 119, 120, 121, 122, 123, 124, 125,
    126, 127, 128, 129, 130, 131, 132, 133, 134, 135,
    136, 137, 138, 139, 140, 141, 142, 143, 144, 145,
]

# SCENARIO_D: serwis — agitation/ripple during maintenance, then returns
SCENARIO_D = [
    100, 99, 101, 100, 98, 102, 100, 99, 101, 100,
    500, 800, 1200, 600, 900, 1100, 700, 500, 300, 200,  # maintenance turbulence
    105, 101, 99, 100, 98, 102, 100, 101, 99, 100,
    100, 98, 102, 100, 99, 101, 98, 102, 100, 99,
    100, 98, 102, 100, 99, 101, 98, 102, 100, 99,
]

# SCENARIO_E: slow leak developing — step increase then stabilises at new level
SCENARIO_E = [
    100, 99, 101, 100, 102, 98, 100, 101, 99, 100,  # baseline 100
    110, 112, 111, 113, 110, 112, 111, 110, 113, 112,  # subtle step up ~110
    150, 152, 148, 151, 149, 150, 152, 148, 151, 150,  # bigger step ~150
    200, 201, 199, 202, 198, 200, 201, 199, 202, 200,  # serious step ~200
    200, 201, 199, 202, 198, 200, 201, 199, 202, 200,  # sustained at 200
]

# ── ACTIVE SCENARIO: change this to switch ──────────────────────────
FLOW_RATES = SCENARIO_A

# ═══════════════════════════════════════════════════════════════════


def zone(score: float) -> tuple[str, str, str]:
    """Returns (label, color_code, symbol)."""
    RESET = "\033[0m"
    if score < ZONE_GREEN:
        return "GREEN",  "\033[32m", "●"
    if score < ZONE_YELLOW:
        return "YELLOW", "\033[33m", "▲"
    return              "RED",    "\033[31m", "■"


def simulate(rates, initial_ema, alpha, clamp, p1, p2):
    ema   = initial_ema
    score = 0.0
    rows  = []

    for i, rate in enumerate(rates):
        if i == 0:
            rows.append((i, rate, rate, ema, score))
            continue

        # 1. Alarm uses EMA from PREVIOUS step
        d = (rate - ema) / ema if ema > 0 else 0.0
        if d > 0:
            instant = p1 * 100.0 * math.log2(1.0 + d)
            decay   = p2 / (1.0 + d)
            score   = score * (1.0 - decay) + instant
        else:
            factor = min(1.0, p2 * (1.0 + abs(d)))
            score  = max(0.0, score * (1.0 - factor))

        # 2. Update EMA with clamped rate
        lo      = ema * (1.0 - clamp)
        hi      = ema * (1.0 + clamp)
        clamped = min(max(rate, lo), hi)
        ema     = ema + alpha * (clamped - ema)

        rows.append((i, rate, clamped, ema, score))

    return rows


def print_table(rows, initial_ema, alpha, clamp, p1, p2):
    RESET = "\033[0m"
    # ── header ──
    print(f"\n{'═'*72}")
    print(f"  EMA₀={initial_ema}  α={alpha}  clamp±{clamp*100:.0f}%  "
          f"p1={p1}  p2={p2}  "
          f"zones: GREEN<{ZONE_GREEN}  YELLOW<{ZONE_YELLOW}  RED≥{ZONE_YELLOW}")
    print(f"{'═'*72}")
    print(f"  {'#':>4}  {'rate':>7}  {'clamped':>8}  {'EMA':>8}  {'score':>8}  zone")
    print(f"  {'-'*62}")

    for step, rate, clamped, ema, score in rows:
        label, clr, sym = zone(score)
        clamped_str = f"{clamped:>8.1f}" if clamped != rate else "        ="
        print(f"{clr}  {step:>4}  {rate:>7.1f}  {clamped_str}  "
              f"{ema:>8.2f}  {score:>8.2f}  {sym} {label}{RESET}")

    print(f"{'═'*72}\n")


if __name__ == "__main__":
    rows = simulate(FLOW_RATES, INITIAL_EMA, ALPHA, CLAMP, P1, P2)
    print_table(rows, INITIAL_EMA, ALPHA, CLAMP, P1, P2)