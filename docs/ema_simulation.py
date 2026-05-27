#!/usr/bin/env python3
"""
Symulacja algorytmu EMA dla systemu ATO.
Odzwierciedla logikę z water_algorithm.cpp / algorithm_config.h.

Uruchomienie:
    python3 docs/ema_simulation.py
"""

# ---- Parametry (mirror algorithm_config.h defaults) ----
EMA_ALPHA            = 0.20
VOL_YELLOW_PCT       = 30
VOL_RED_PCT          = 60
RATE_YELLOW_PCT      = 40
RATE_RED_PCT         = 80
MIN_BOOTSTRAP        = 5

# ---- rapidRefillCount — zamrożenie EMA podczas serii szybkich dolewek ----
# Jeśli interval < EMA_int * RAPID_REFILL_RATIO → dolewka uznana za "rapid"
# Po RAPID_REFILL_TRIGGER kolejnych rapid → EMA_rate i EMA_int zamrożone
RAPID_REFILL_RATIO   = 0.30
RAPID_REFILL_TRIGGER = 1


def clamp_pct(v):
    return max(-127, min(127, round(v)))


def dev_pct(current, ema_val, bootstrap_count):
    if ema_val < 0.001 or bootstrap_count < MIN_BOOTSTRAP:
        return 0
    return clamp_pct((current - ema_val) / ema_val * 100)


def alert_level(dev_vol, dev_rate, bootstrap_count):
    if bootstrap_count < MIN_BOOTSTRAP:
        return 0
    abs_vol  = abs(dev_vol)
    abs_rate = abs(dev_rate)
    if abs_vol >= VOL_RED_PCT or abs_rate >= RATE_RED_PCT:
        return 2
    if abs_vol >= VOL_YELLOW_PCT or abs_rate >= RATE_YELLOW_PCT:
        return 1
    return 0


def run_simulation(events, alpha=EMA_ALPHA, label=""):
    """
    events: lista krotek (volume_ml, interval_s)
    Zwraca listę wyników rekordów.
    """
    print(f"\n{'='*80}")
    print(f"SYMULACJA: {label}  alpha={alpha}  "
          f"rapid_ratio={RAPID_REFILL_RATIO}  rapid_trigger={RAPID_REFILL_TRIGGER}")
    print(f"{'='*80}")
    header = (f"{'#':>3}  {'vol[ml]':>8}  {'int[h]':>7}  {'rate[ml/h]':>11}  "
              f"{'EMA_vol':>8}  {'EMA_rate':>9}  {'dVol%':>6}  {'dRate%':>7}  "
              f"{'alert':>6}  {'rfc':>4}  {'frz':>4}")
    print(header)
    print("-" * 95)

    ema_vol            = 0.0
    ema_int            = 0.0
    ema_rate           = 0.0
    bootstrap          = 0
    rapid_refill_count = 0

    records = []

    for i, (vol, interval_s) in enumerate(events, 1):
        rate = (vol / interval_s * 3600) if interval_s > 0 else 0.0

        # Wykryj rapid refill względem aktualnego EMA_int
        is_rapid = (bootstrap > 0 and ema_int > 0
                    and interval_s < ema_int * RAPID_REFILL_RATIO)
        if is_rapid:
            rapid_refill_count += 1
        else:
            rapid_refill_count = 0

        # Zamrożenie EMA_rate i EMA_int gdy za dużo kolejnych rapid
        freeze_ema = is_rapid and (rapid_refill_count >= RAPID_REFILL_TRIGGER)

        if bootstrap == 0:
            ema_vol  = float(vol)
            ema_int  = float(interval_s)
            ema_rate = rate
        else:
            ema_vol = alpha * vol + (1 - alpha) * ema_vol
            if not freeze_ema:
                ema_int  = alpha * interval_s + (1 - alpha) * ema_int
                ema_rate = alpha * rate        + (1 - alpha) * ema_rate

        bootstrap = min(bootstrap + 1, 255)

        dv = dev_pct(vol,  ema_vol,  bootstrap)
        dr = dev_pct(rate, ema_rate, bootstrap)
        al = alert_level(dv, dr, bootstrap)

        ALERT_STR = ["   OK", " ŻÓŁT", "  CZE"]
        alert_s = ALERT_STR[al] if al < 3 else "?????"
        frz_s   = " TAK" if freeze_ema else "  --"

        print(f"{i:>3}  {vol:>8}  {interval_s/3600:>7.4f}  {rate:>11.2f}  "
              f"{ema_vol:>8.1f}  {ema_rate:>9.3f}  {dv:>+6}  {dr:>+7}  "
              f"{alert_s}  {rapid_refill_count:>4}  {frz_s}")

        records.append({
            "i": i, "vol": vol, "interval_s": interval_s, "rate": rate,
            "ema_vol": ema_vol, "ema_rate": ema_rate,
            "dev_vol": dv, "dev_rate": dr, "alert": al, "bootstrap": bootstrap,
            "rapid_refill_count": rapid_refill_count, "freeze_ema": freeze_ema,
        })

    return records


# Parametry sekwencji testowej
DOSE_ML = 200           # objętość dolewki (stała dla całej sekwencji)
NORMAL_INTERVAL = 7200  # odstęp [s] w normalnych warunkach
STABLE_PREFIX = 3       # liczba normalnych dolewek przed zakłóceniami
STABLE_SUFFIX = 15      # liczba normalnych dolewek po zakłóceniach

# Serie zakłóceń — każda lista to osobna tabela wynikowa
disturbance_intervals = [
# first set
    [300],
    [300, 300],
    [300, 300, 300],
# second set
    [800],
    [800, 800],
    [800, 800, 800],
# third set 
    [1200],
    [1200, 1200],
    [1200, 1200, 1200],
# fifth set 
    [2000],
    [2000, 2000],
    [2000, 2000, 2000],

]

def make_test_events(disturbance, dose_ml=DOSE_ML,
                     normal_interval=NORMAL_INTERVAL,
                     prefix=STABLE_PREFIX, suffix=STABLE_SUFFIX):
    stable = (dose_ml, normal_interval)
    return (
        [stable] * prefix
        + [(dose_ml, ivl) for ivl in disturbance]
        + [stable] * suffix
    )


if __name__ == "__main__":
    for disturbance in disturbance_intervals:
        alpha_test_events = make_test_events(disturbance)

        print(f"\n{'='*80}")
        print(f"ZAKŁÓCENIA: {disturbance}  ({len(disturbance)} dolewek)")
        print(f"  events: {STABLE_PREFIX}× normal + {len(disturbance)}× zakłócenie + {STABLE_SUFFIX}× normal")
        print(f"  RAPID_REFILL_RATIO={RAPID_REFILL_RATIO}  RAPID_REFILL_TRIGGER={RAPID_REFILL_TRIGGER}")
        print(f"{'='*80}")

        for a in [0.40]:
            r = run_simulation(alpha_test_events, alpha=a, label=f"alpha={a}")
            first_alert = next((rec for rec in r if rec["alert"] > 0), None)
            if first_alert:
                print(f"  → alpha={a}: pierwszy alert przy dolewce #{first_alert['i']} "
                      f"(dev_rate={first_alert['dev_rate']:+d}%)")
            else:
                print(f"  → alpha={a}: brak alertów")

        print()
